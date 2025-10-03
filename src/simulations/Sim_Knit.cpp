#include "Sim_Knit.hpp"
#include "Geometry_Knit.hpp"
#include "MaterialProperties.hpp"
#include "CombinedOperator_Parametric.hpp"
#include "GrowthHelper.hpp"
#include "ArgumentParser.hpp"
#include <igl/boundary_loop.h>
#include "Parametrizer.hpp"
#include "HLBFGS_Wrapper_Parametrized.hpp"


template<typename tMesh>
class Parametrizer_Knit_Boundary : public Parametrizer<tMesh>
// class works w/ any mesh type tMesh
// it inherits from generic class Parametrizer<tMesh> so it can use or override its methods 

{
public:
    using Parametrizer<tMesh>::mesh; // to avoid having to type this-> all the time
    using Parametrizer<tMesh>::data; // to avoid having to type this-> all the time
    //dont understand !! what does :: mean? 
protected:
    const Real springConstant;

    // Containers for precomputed values that don't change after initialization
    Eigen::ArrayXd boundaryMask = Eigen::ArrayXd::Zero(mesh.getNumberOfVertices());

    //this is an array the same size as the number of mesh vertices 
    //its initialised to zero 
    //used to mark boundaries with 1.0 
public:
    Parametrizer_Knit_Boundary(tMesh & mesh_in, const Real springConstant, const Real res):
    Parametrizer<tMesh>(mesh_in),
    springConstant(springConstant * res)
    {
        assert(springConstant >= 0.0);
        //makes sure the final spring constant is not negative 
        Eigen::ArrayXi boundaryVertices;
        igl::boundary_loop(mesh.getTopology().getFace2Vertices(), boundaryVertices);
        // uses libigl's boundary_loop function to find all vertices on the boundary loop of the mesh 
        // calls mesh.getTopology().getFace2Vertices() which gives face-vertex connectivity 
        for (const auto i : boundaryVertices) {
            boundaryMask(i) = 1; 
            // tags vertices on the boundary 
        }
    }

    void initSolution(const Eigen::Ref<const Eigen::VectorXd>, const bool) override
    {
        // do nothing : we are using the mesh data
    }

    Real * getDataPointer() const override
    {
        return mesh.getDataPointer();
    }

    int getNumberOfVariables() const override
    {
        return 3 * mesh.getNumberOfVertices() + mesh.getNumberOfEdges(); // DCS energy minimization
        // each vertex has 3 coords and there's one variable per edge --- shows how big the optimization space is 
    }

    void updateSolution() override
    {
        mesh.updateDeformedConfiguration();
    }

    Real computeEnergyContribution() override
    {
        // here we have a function that penalizes
        const int nVertices = mesh.getNumberOfVertices();
        const auto vertices = mesh.getCurrentConfiguration().getVertices();
        const auto rvertices = mesh.getRestConfiguration().getVertices();

        Eigen::MatrixXd displacements = vertices - rvertices;
        Eigen::ArrayXd springLengths = displacements.rowwise().norm().array();
        return 0.5 * springConstant * (springLengths * springLengths * boundaryMask).sum(); // energy functional being minimized 
        //total spring energy stored in the boundary vertex displacements 
    }

    // multivariable optimization problems 
    void updateGradient(const int nVars, const Eigen::Ref<const Eigen::VectorXd> energyGradient, Real * const grad_ptr) override
    {
        assert(nVars == getNumberOfVariables());

        // set grad_ptr equal to the actual gradient
        for (int i=0; i<nVars; ++i) {
            grad_ptr[i] = energyGradient(i);
        }

        const int nVertices = mesh.getNumberOfVertices();
        Eigen::Map<Eigen::VectorXd> gradVertices_x(grad_ptr, nVertices);
        Eigen::Map<Eigen::VectorXd> gradVertices_y(grad_ptr + nVertices, nVertices);
        Eigen::Map<Eigen::VectorXd> gradVertices_z(grad_ptr + 2*nVertices, nVertices);

        const auto vertices = mesh.getCurrentConfiguration().getVertices();
        const auto rvertices = mesh.getRestConfiguration().getVertices();

        // add the contribution from the mollifier
        Eigen::MatrixXd displacements = vertices - rvertices;
        Eigen::VectorXd springLengths = displacements.rowwise().norm();
        Eigen::MatrixXd normalizedDisplacements = displacements.rowwise().normalized();
        Eigen::ArrayXd springMagnitudes = springConstant * springLengths.array() * boundaryMask;

        gradVertices_x += (springMagnitudes * normalizedDisplacements.col(0).array()).matrix();
        gradVertices_y += (springMagnitudes * normalizedDisplacements.col(1).array()).matrix();
        gradVertices_z += (springMagnitudes * normalizedDisplacements.col(2).array()).matrix();
    }
};


void Sim_Knit::init()
{}


void Sim_Knit::run()
{
    runKnit();
}


void Sim_Knit::runKnit()
{
    const std::string filename = parser.parse<std::string>("-filename", "knit.txt");
    tag = filename.substr(0, filename.length() - 4);

    // Initialize geometry/mesh
    const Real res = parser.parse<Real>("-res", 0.1);
    KnitPlate geometry(parser);
    mesh.init(geometry);

    // Define material parameters
    const Real E = parser.parse<Real>("-E", 1.0); //youngsmodulus 
    const Real nu = parser.parse<Real>("-nu", 0.4); //poissonratio
    const Real h = parser.parse<Real>("-h", 0.5); //thickness 

    const Real boundarySpringConstant = parser.parse<Real>("-boundarySpringConstant", 0.0);

    MaterialProperties_Iso_Constant matprop(E, nu, h);
    CombinedOperator_Parametric<tMesh, Material_Isotropic> engOp(matprop);//energyoperator 

    // Mesh info
    //auto rvertices = mesh.getRestConfiguration().getVertices();
    //auto cvertices = mesh.getCurrentConfiguration().getVertices();
    Eigen::Ref<Eigen::MatrixXd> rvertices = mesh.getRestConfiguration().getVertices();
    Eigen::Ref<Eigen::MatrixXd> cvertices = mesh.getCurrentConfiguration().getVertices();

    auto faces = mesh.getTopology().getFace2Vertices();
    const int nVertices = rvertices.rows();
    const int nFaces = faces.rows();
    tVecMat2d & bforms = mesh.getRestConfiguration().getSecondFundamentalForms();

    // Get geometric data
    Eigen::VectorXd stitchData = geometry.getCurvatureData(rvertices, faces);

    // Define constant natural curvatures
    const Real xCurv = parser.parse<Real>("-xCurv", -1.0); //knit 
    const Real yCurv = parser.parse<Real>("-yCurv", 1.0); //knit  
    Eigen::VectorXd curvatureAngles = Eigen::VectorXd::Constant(nFaces, 0.0);
    Eigen::VectorXd curvatures_p = xCurv * stitchData; 
    Eigen::VectorXd curvatures_o = yCurv * stitchData; 

    // Curve incrementally
    const int nSteps = parser.parse<int>("-nSteps", 10);

    
    //Set boundary conditions to set vertices on left and right edges 
    auto boundaryConditions = mesh.getBoundaryConditions().getVertexBoundaryConditions();

    //changed to top and bottom (2 october 2025) 
    for (int i = 0; i < nVertices; ++i) {
        if (rvertices(i, 1) < rvertices.col(1).minCoeff()+1e-6) { // left edge (bottom)
            boundaryConditions(i,0)=true;
            boundaryConditions(i,1)=true;
            boundaryConditions(i,2)=true;
        } else if (rvertices(i, 1) > rvertices.col(1).maxCoeff()-1e-6) { // right edge (top) 
            boundaryConditions(i, 0)=true; 
            boundaryConditions(i, 1)=true;
            boundaryConditions(i, 2)=true;
        }
    }
    //
    

    for (int step = 0; step <= nSteps; step++) {
        const Real s = (1.0 * step) / (1.0 * nSteps); 
    

        if (step > 0) {
            // Update bform on faces
            GrowthHelper<tMesh>::computeBbarsOrthoGrowthViaBbar(mesh, curvatureAngles, s * curvatures_p, s * curvatures_o, bforms); //update curvature in 2nd fundamental form 

            addNoiseToVertices_c<2>(0.01 * h);

            // Minimize energy
            if (boundarySpringConstant == 0.0) { // Free boundary condition
                Real eps = 1e-2;
                minimizeEnergy(engOp, eps); 
            } else { // Boundary springs
                Real eps = 1e-3;

                Parametrizer_Knit_Boundary<tMesh> parametrizer(mesh, boundarySpringConstant, res);
                HLBFGS_Methods::HLBFGS_EnergyOp_Parametrized<tMesh, Parametrizer_Knit_Boundary, true> hlbfgs_wrapper(mesh, engOp, parametrizer);

                const Real epsMin = std::numeric_limits<Real>::epsilon();
                hlbfgs_wrapper.minimize(tag + "_diagnostics.dat", epsMin);
                eps = hlbfgs_wrapper.get_lastnorm();

                { // store energies
                    std::vector<std::pair<std::string, Real>> energies;
                    engOp.addEnergy(energies);
                    currentEnergies = energies;
                    FILE * f = fopen((tag+"_energies.dat").c_str(), "a");
                    fprintf(f, "Step %d (phase 1):\n", step);
                    for(const auto & eng : energies)
                    {
                        fprintf(f, "%s \t\t %10.10e\n", eng.first.c_str(), eng.second);
                    }
                    fclose(f);
                }
            }
        }

        dump(tag + "_f" + std::to_string(s), "stitchData", stitchData);
    }

    const int n_pulling_steps = parser.parse<int>("-nPullingSteps", 10);
    const Real pull_total = parser.parse<Real>("-pullAmount", 5);
    const Real move_amount = pull_total / (2.0 * n_pulling_steps); //how far to move each boundary per step 
    std::cout << move_amount << " move amount" << std::endl;


    // Pulling steps
    for (int step2 = 0; step2 < n_pulling_steps; ++step2) {
        const Real s = static_cast<Real>(step2) / n_pulling_steps;

    
        // === compute left/right boundaries at this step -- changed to top and bottom edges (2 october 2025) ===
        std::vector<int> leftBoundary, rightBoundary;
        Real xmin = cvertices.col(1).minCoeff();
        Real xmax = cvertices.col(1).maxCoeff();
        Real boundary_band=move_amount;

        for (int i = 0; i < nVertices; ++i) {
            if (std::abs(cvertices(i, 1)-xmin) < boundary_band) { 
                leftBoundary.push_back(i);
            } else if (std::abs(cvertices(i, 1)-xmax) < boundary_band) {
                rightBoundary.push_back(i);
            }
        }

        std::cout << "Step " << step2 << ": moving "
        << leftBoundary.size() << " left, "
        << rightBoundary.size() << " right." << std::endl;

        std::cout << move_amount << " move amount" << std::endl;
        // === Pull vertices ===
        for (int i : leftBoundary) {
            //rvertices(i, 0) += move_amount;
            cvertices(i, 1) += move_amount;
        }
        for (int i : rightBoundary) {
            //rvertices(i, 0) -= move_amount;
            cvertices(i, 1) -= move_amount;
        }

        // Step tag
        std::cout << "=== Step" << step2 << " ===" << std::endl;

        // Global mesh width
        Real xmin_global = cvertices.col(1).minCoeff();
        Real xmax_global = cvertices.col(1).maxCoeff();
        Real width_global = xmax_global - xmin_global;

        // Boundary averages
        Real left_avg = 0.0, right_avg = 0.0;
        for (int i : leftBoundary)  left_avg  += cvertices(i, 1);
        for (int i : rightBoundary) right_avg += cvertices(i, 1);
        left_avg  /= leftBoundary.size();
        right_avg /= rightBoundary.size();
        Real width_avg = right_avg - left_avg;

        // Individual sample points (optional)
        Real left_sample = cvertices(leftBoundary[0], 1);
        Real right_sample = cvertices(rightBoundary[0], 1);
        Real width_sample = right_sample - left_sample;

        // Print all comparisons
        std::cout << "Global  ymin: " << xmin_global << ", ymax: " << xmax_global
                << ", height: " << width_global << std::endl;

        std::cout << "Avg     bottom: " << left_avg << ", top: " << right_avg
                << ", height: " << width_avg << std::endl;

        std::cout << "Sample  bottom: " << left_sample << ", top: " << right_sample
                << ", height: " << width_sample << std::endl;


        addNoiseToVertices_c<2>(0.01 * h);

        if (boundarySpringConstant == 0.0) {
            Real eps = 1e-2;
            minimizeEnergy(engOp, eps);
        } else {
            Real eps = 1e-3;
            Parametrizer_Knit_Boundary<tMesh> parametrizer(mesh, boundarySpringConstant, res);
            HLBFGS_Methods::HLBFGS_EnergyOp_Parametrized<tMesh, Parametrizer_Knit_Boundary, true> hlbfgs_wrapper(mesh, engOp, parametrizer);

            const Real epsMin = std::numeric_limits<Real>::epsilon();
            hlbfgs_wrapper.minimize(tag + "_diagnostics.dat", epsMin);
            eps = hlbfgs_wrapper.get_lastnorm();

            std::vector<std::pair<std::string, Real>> energies;
            engOp.addEnergy(energies);
            currentEnergies = energies;
            FILE * f = fopen((tag+"_energies.dat").c_str(), "a");
            fprintf(f, "Step %d:\n", step2);
            for (const auto & eng : energies) {
                fprintf(f, "%s \t\t %10.10e\n", eng.first.c_str(), eng.second);
            }
            fprintf(f, "\n");
            fclose(f);
        }

        /*
        // Step tag
        std::cout << "=== Step after energy min " << step2 << " ===" << std::endl;

        // Global mesh width
        Real xmin_global2 = cvertices.col(0).minCoeff();
        Real xmax_global2 = cvertices.col(0).maxCoeff();
        Real width_global2 = xmax_global - xmin_global;

        // Boundary averages
        Real left_avg2 = 0.0, right_avg2 = 0.0;
        for (int i : leftBoundary)  left_avg2 += cvertices(i, 0);
        for (int i : rightBoundary) right_avg2 += cvertices(i, 0);
        left_avg2  /= leftBoundary.size();
        right_avg2 /= rightBoundary.size();
        Real width_avg2 = right_avg2 - left_avg2;

        // Individual sample points (optional)
        Real left_sample2 = cvertices(leftBoundary[0], 0);
        Real right_sample2 = cvertices(rightBoundary[0], 0);
        Real width_sample2 = right_sample2 - left_sample2;

        // Print all comparisons
        std::cout << "Global  xmin: " << xmin_global2 << ", xmax: " << xmax_global2
                << ", width: " << width_global2 << std::endl;

        std::cout << "Avg     left: " << left_avg2 << ", right: " << right_avg2
                << ", width: " << width_avg2 << std::endl;

        std::cout << "Sample  left: " << left_sample2 << ", right: " << right_sample2
                << ", width: " << width_sample2 << std::endl;
        */

        dump(tag + "_f" + std::to_string(s) + "_pulled", "stitchData", stitchData);
    }
}





