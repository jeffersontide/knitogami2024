import numpy as np
import xml.etree.ElementTree as ET
import base64
import zlib
import os
import re


"""Utility for reading and writing single-component VTP and OBJ files containing 
triangle meshes. VTP files may have data stored per-vertex and per-face as well.

Usage:
    >>> import mesh_data
    >>> data = mesh_data.VTPData("filename.vtp")
    >>> data.writeOBJ("new_filename.obj")


Attributes:
   data.nFaces         number of faces
   data.faces          (F x 3) numpy array of vertices for each face, zero-indexed
   data.nVertices      number of vertices
   data.vertices       (V x 3) numpy array of vertex positions
   data.pointData      python dict with vertex-oriented data, e.g. displacements
   data.cellData       python dict with face-oriented data, e.g. strain
   data.filename       filename of initial VTP file
"""

class MeshData:
    def __init__(self, *args):
        self.filename = args[0] if len(args) > 0 else ""
        self.nVertices = 0
        self.nFaces = 0
        self.vertices = None
        self.faces = None
        self.cellData = {}
        self.pointData = {}


class VTPData(MeshData):
    def __init__(self, filename):
        MeshData.__init__(self, filename)
        self.readVTP()

    def readVTP(self):
        """Create a dict containing information about a given mesh, which is stored
        in a VTP file. This function assumes some things about the mesh (e.g., that
        the mesh has only one component) that may need to be changed if used in other
        contexts.

        XML Structure of .vtp file:

              VTKFile                       # Root of imported ElementTree
                  PolyData
                  Piece                     # NumberOfPoints, NumberOfVerts, ...
                      PointData             # (no information)
                      CellData
                          DataArray (1)     # type, Name, format="appended", ...
                          DataArray (2)
                          ...
                      Points
                          DataArray         # list of vertex XYZ positions
                      Verts
                          DataArray (1)     # (no information)
                          DataArray (2)     # (no information)
                      Lines
                          DataArray (1)     # (no information)
                          DataArray (2)     # (no information)
                      Strips
                          DataArray (1)     # (no information)
                          DataArray (2)     # (no information)
                      Polys
                          DataArray (1)     # connectivity: lists vertices (zero-indexed)
                          DataArray (2)     # offsets: lists multiples of 3 from 3 to (3 * nTriangles)
                  AppendedData              # encoding="base64"
                      '_CAAAAACAAAAISgAAjHoAAHZ6AAB7egAAinoAAHJ6AAB9egAAdHoAA...'


        Resources for parsing appended data:
            https://vtk.org/Wiki/VTK_XML_Formats
            https://vtk.org/wp-content/uploads/2015/04/file-formats.pdf
            https://public.kitware.com/pipermail/vtk-developers/2017-March/034796.html
        """

        if not os.path.exists(self.filename):
            raise RuntimeError(f"Can't find VTP file: {self.filename}")

        # Create ElementTree of VTP (XML) file
        tree = ET.parse(self.filename)
        root = tree.getroot()

        # Get encoded/compressed string corresponding to appended data
        appendedData = root.find('AppendedData').text.strip()[1:] # Remove leading underscore

        # Get mesh info and data
        self.nVertices = int(root.find('PolyData').find('Piece').get('NumberOfPoints'))
        self.nFaces = int(root.find('PolyData').find('Piece').get('NumberOfPolys'))

        vertexData = root.find('PolyData').find('Piece').find('Points').find('DataArray')
        self.vertices = VTPData.getData(appendedData, int(vertexData.get('offset')), vertexData.get('type'), NumberOfComponents=3)

        faceData = root.find('PolyData').find('Piece').find('Polys').findall('DataArray')
        for data in faceData:
            if data.get('Name') == 'connectivity':
                self.faces = VTPData.getData(appendedData, int(data.get('offset')), data.get('type'), NumberOfComponents=3)
                break

        cellDataNode = root.find('PolyData').find('Piece').find('CellData')
        if cellDataNode is not None:
            for field in cellDataNode:
                self.cellData[field.get('Name')] = VTPData.getData(appendedData, int(field.get('offset')), field.get('type'), VTPData.intOrNone(field.get('NumberOfComponents')))
                if len(self.cellData[field.get('Name')]) > self.nFaces:
                    self.cellData[field.get('Name')] = self.cellData[field.get('Name')][:self.nFaces]

        pointDataNode = root.find('PolyData').find('Piece').find('PointData')
        if pointDataNode is not None:
            for field in pointDataNode:
                self.pointData[field.get('Name')] = VTPData.getData(appendedData, int(field.get('offset')), field.get('type'), VTPData.intOrNone(field.get('NumberOfComponents')))
                if len(self.pointData[field.get('Name')]) > self.nVertices:
                    self.pointData[field.get('Name')] = self.pointData[field.get('Name')][:self.nVertices]

    @staticmethod
    def b64chars(nBytes):
        """Calculates the number of characters in an encoded base64 string
        corresponding to a given number of bytes.
        """
        return int(4 * np.ceil(nBytes / 3.0))

    @staticmethod
    def getData(data, offset, numericType, NumberOfComponents=1):
        """Extract data from a given offset in a zlib-compressed, base64-encoded
        `data` string.
        """
        i = offset
        block_sizes = []

        # Get header for DataArray
        header_length = VTPData.b64chars(3 * 4)
        header = data[i:i+header_length] # 4 bytes per UInt32 number
        header_info = np.frombuffer(base64.b64decode(header), dtype='<u4')
        i += header_length

        # If no data, return `None`
        if header_info[0] == 0:
            return None

        # Get length of each block in DataArray
        number_length = VTPData.b64chars(header_info[0] * 4) # (# blocks) of UInt32 numbers
        block_sizes = np.frombuffer(base64.b64decode(data[i:i+number_length]), dtype='<u4')
        i += number_length

        # Decode DataArray information from base64 string
        if len(block_sizes > 0):
            data_length = VTPData.b64chars(np.sum(block_sizes))
            body = data[i:i+data_length]
            z = bytearray(base64.b64decode(body))
            i += data_length

        # Decompress information block-by-block and store byte data in variable `d`
        j = 0
        d = b''
        for block_size in block_sizes:
            d += zlib.decompress(bytes(z[j:j+block_size]))
            j += block_size

        # Decode data and return Numpy array

        # String data
        if numericType == "String":
            try:
                data_array = d.rstrip(' \x00')
                return data_array
            except:
                data_array = d.rstrip(b' \x00')
                return data_array

        # Bit data
        if numericType == "Bit":
            bits = np.unpackbits(np.frombuffer(d, dtype=np.uint8))
            if NumberOfComponents:
                data_array = np.squeeze(np.reshape(bits[:(len(bits) // int(NumberOfComponents)) * int(NumberOfComponents)], (len(bits) // int(NumberOfComponents), int(NumberOfComponents))))
            return data_array

        # Numeric data
        numericType = np.dtype(numericType.lower())
        data_array = np.frombuffer(d, dtype=numericType)

        if NumberOfComponents:
            return np.squeeze(np.reshape(data_array, (len(data_array) // NumberOfComponents, int(NumberOfComponents))))
        else:
            return data_array

    @staticmethod
    def intOrNone(var):
        """Cast to an integer if possible, otherwise return None.
        """
        try:
            return int(var)
        except:
            return None

    def writeOBJ(self, *args):
        filename = self.filename if len(args) == 0 else args[0]
        if filename.lower().endswith(".vtp"):
            filename = filename[:-4]
        filename += ".obj"

        with open(filename, "w+") as f:
            for i in range(self.nVertices):
                f.write("v {0} {1} {2}\n".format(
                    self.vertices[i, 0],
                    self.vertices[i, 1],
                    self.vertices[i, 2]
                ))
            for i in range(self.nFaces):
                f.write("f {0} {1} {2}\n".format(
                    self.faces[i, 0] + 1,
                    self.faces[i, 1] + 1,
                    self.faces[i, 2] + 1
                ))


class OBJData(MeshData):
    def __init__(self, *args):
        if len(args) == 1: # filename
            MeshData.__init__(self, args[0])
            self.readOBJ()
        elif len(args) >= 2: # vertices, faces, filename
            MeshData.__init__(self)
            self.vertices = args[0]
            self.faces = args[1]
            self.nVertices = len(self.vertices)
            self.nFaces = len(self.faces)
            self.filename = f"{args[2]}.obj" if len(args) > 2 else "temp.obj"
        else:
            raise

    def writeOBJ(self, *args):
        filename = self.filename if len(args) == 0 else args[0]
        filename += "" if filename.lower().endswith(".obj") else ".obj"
        with open(filename, "w+") as f:
            for i in range(self.nVertices):
                f.write("v {0} {1} {2}\n".format(
                    self.vertices[i, 0],
                    self.vertices[i, 1],
                    self.vertices[i, 2]
                ))
            for i in range(self.nFaces):
                f.write("f {0} {1} {2}\n".format(
                    self.faces[i, 0] + 1,
                    self.faces[i, 1] + 1,
                    self.faces[i, 2] + 1
                ))

    def readOBJ(self):
        with open(self.filename, "r") as f:
            vertices = []
            faces = []
            for line in f:
                if line.startswith("v"):
                    vertices.append([float(r) for r in line.split()[1:]])
                elif line.startswith("f"):
                    faces.append([int(v) for v in line.split()[1:]])

        self.vertices = np.array(vertices, dtype=float)
        self.faces = np.array(faces, dtype=int)

        self.nVertices = self.vertices.shape[0]
        self.nFaces = self.faces.shape[0]