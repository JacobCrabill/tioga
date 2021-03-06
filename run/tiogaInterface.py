#! /usr/bin/env python
#
# File: tiogaInterface.py
# Authors: Jacob Crabill
# Last Modified: 9/03/2017

__version__ = "1.00"


# ==================================================================
# Standard Python modules

import sys
import os
import copy
import string
import types

#Extension modules
#sys.path.append(os.path.abspath(os.getenv('PYTHONPATH')))
#sys.path.append(os.path.abspath(os.getenv('PYTHONPATH')+'/numpy'))

import numpy as np

CONVERT_DIR = '/home/jcrabill/zefr/external/'
sys.path.append(CONVERT_DIR)

from convert import *

# Try to import the MPI.COMM_WORLD.module
try:
    from mpi4py import MPI
    _parallel = True

except ImportError:
    _parallel = False
    raise


try:
    import tioga as tg
except ImportError:
    print("Import Error: TIOGA connectivity module")
    raise

class Tioga:

    def __init__(self,gridID,nGrids):

        self.nGrids = nGrids
        self.ID = gridID

        Comm = MPI.COMM_WORLD
        rank = Comm.Get_rank()
        nproc = Comm.Get_size()

        self.gridComm = Comm.Split(gridID,rank)
        self.gridRank = self.gridComm.Get_rank()
        self.gridSize = self.gridComm.Get_size()

        tg.tioga_init_(Comm)

        self.name = 'tioga'

    # Initial grid preprocessing
    def preprocess(self):
        tg.tioga_preprocess_grids_()

    # Perform the full domain connectivity
    def performConnectivity(self):
        tg.tioga_performconnectivity_()

    # Perform just the donor/fringe point connecitivity (using current blanking)
    def performPointConnectivity(self):
        tg.tioga_do_point_connectivity()

    # For high-order codes: First part of unblank procedure (t^{n+1} blanking)
    def unblankPart1(self):
        tg.tioga_unblank_part_1()

    # For high-order codes: Second part of unblank procedure (t^n blanking + union)
    def unblankPart2(self):
        tg.tioga_unblank_part_2(self.nfields)

    # Interpolate solution and send/receive all data
    def exchangeSolution(self):
        tg.tioga_dataupdate_ab(self.nfields, 0)

    # Interpolate solution gradient and send/receive all data
    def exchangeGradient(self):
        tg.tioga_dataupdate_ab(self.nfields, 1)

    def sifInitialize(self,properties,conditions):
        self.ndims = int(properties['ndims'])
        self.nfields = int(properties['nfields'])
        self.motion = int(properties['moving-grid'])
        self.gridScale = conditions['meshRefLength']

        try:
            self.useGpu = int(properties['use-gpu'])
        except:
            self.useGpu = False

        # Have TIOGA do any additional setup on input parameters
        #tg.sifinit(dt,Re,mach_fs)

    # sifInit called first to setup simulation input
    def initData(self,gridData,callbacks):
        self.gridData = gridData
        self.callbacks = callbacks

        # Get pointers to grid data
        btag = gridData['bodyTag'][0]
        xyz = arrayToDblPtr(gridData['grid-coordinates'][0])
        c2v = arrayToIntPtr(gridData['hexaConn'][0])
        iblank = arrayToIntPtr(gridData['iblanking'][0])

        overNodes = arrayToIntPtr(gridData['obcnode'][0])
        wallNodes = arrayToIntPtr(gridData['wallnode'][0])

        c2f = arrayToIntPtr(gridData['cell2face'][0])
        f2c = arrayToIntPtr(gridData['face2cell'][0])

        iblank = arrayToIntPtr(gridData['iblanking'][0])
        iblank_face = arrayToIntPtr(gridData['iblank-face'][0])
        iblank_cell = arrayToIntPtr(gridData['iblank-cell'][0])

        overFaces = arrayToIntPtr(gridData['overset-faces'][0])
        wallFaces = arrayToIntPtr(gridData['wall-faces'][0])

        mpiFaces = arrayToIntPtr(gridData['mpi-faces'][0])
        mpiProcR = arrayToIntPtr(gridData['mpi-right-proc'][0])
        mpiFidR = arrayToIntPtr(gridData['mpi-right-id'][0])

        f2v = arrayToIntPtr(gridData['faceConn'][0])

        # Extract metadata
        nCellTypes = 1
        nFaceTypes = 1
        nnodes = gridData['grid-coordinates'][0].shape[0]
        ncells = gridData['hexaConn'][0].shape[0]
        nvert = gridData['hexaConn'][0].shape[1]
        nfaces = gridData['faceConn'][0].shape[0]
        nvertf = gridData['faceConn'][0].shape[1]

        nover = gridData['obcnode'][0].shape[0]
        nwall = gridData['wallnode'][0].shape[0]

        nOverFace = gridData['overset-faces'][0].shape[0]
        nWallFace = gridData['wall-faces'][0].shape[0]
        nMpiFace = gridData['mpi-faces'][0].shape[0]

        gridType = gridData['gridCutType']

        tg.tioga_registergrid_data_(btag, nnodes, xyz, iblank,
            nwall, nover, wallNodes, overNodes, nCellTypes, nvert,
            ncells, c2v)

        tg.tioga_setcelliblank_(iblank_cell)

        tg.tioga_register_face_data_(gridType, f2c, c2f, iblank_face,
            nOverFace, nWallFace, nMpiFace, overFaces, wallFaces,
            mpiFaces, mpiProcR, mpiFidR, nFaceTypes, nvertf, nfaces, f2v);

        # Get solver callbacks
        get_nodes_per_cell = callbacks['nodesPerCell']
        get_nodes_per_face = callbacks['nodesPerFace']
        get_receptor_nodes = callbacks['receptorNodes']
        get_face_nodes = callbacks['faceNodes']
        donor_inclusion_test = callbacks['donorInclusionTest']
        convert_to_modal = callbacks['convertToModal']
        donor_frac = callbacks['donorFrac']

        get_q_spt = callbacks['get_q_spt']
        get_q_fpt = callbacks['get_q_fpt']
        get_dq_spt = callbacks['get_dq_spt']
        get_dq_fpt = callbacks['get_dq_fpt']
        get_q_spts = callbacks['get_q_spts']
        get_dq_spts = callbacks['get_dq_spts']

        tg.tioga_set_highorder_callback_(get_nodes_per_cell,
            get_receptor_nodes, donor_inclusion_test, donor_frac,
            convert_to_modal)

        tg.tioga_set_ab_callback_(get_nodes_per_face, get_face_nodes,
            get_q_spt, get_q_fpt, get_dq_spt, get_dq_fpt, get_q_spts,
            get_dq_spts)

        if self.motion:
            gridV = arrayToDblPtr(gridData['gridVel'][0])
            offset = arrayToDblPtr(gridData['rigidOffset'][0])
            Rmat = arrayToDblPtr(gridData['rigidRotMat'][0])
            tg.tioga_register_moving_grid_data(gridV,offset,Rmat)

        if self.useGpu:
            donorFromDevice = callbacks['donorDataDevice']
            fringeToDevice = callbacks['fringeDataToDevice']
            unblankToDevice = callbacks['unblankToDevice']
            faceNodesGPU = callbacks['faceNodesGPU']
            cellNodesGPU = callbacks['cellNodesGPU']
            qSpts_d = callbacks['q_spts_d']
            dqSpts_d = callbacks['dq_spts_d']
            nWeightsGPU = callbacks['nWeightsGPU']
            weightsGPU = callbacks['weightsGPU']

            tg.tioga_set_ab_callback_gpu_(donorFromDevice, fringeToDevice,
                unblankToDevice, qSpts_d, dqSpts_d, faceNodesGPU, cellNodesGPU,
                nWeightsGPU, weightsGPU)

            coords_d = arrayToDblPtr(gridData['nodesGPU'][0])
            ecoords_d = arrayToDblPtr(gridData['eleCoordsGPU'][0])
            iblankCell_d = arrayToIntPtr(gridData['iblankCellGPU'][0])
            iblankFace_d = arrayToIntPtr(gridData['iblankFaceGPU'][0])

            tg.tioga_set_device_geo_data(coords_d, ecoords_d, iblankCell_d,
                iblankFace_d)

            tb.tioga_set_stream_handle(gridData['cuStream'],gridData['cuEvent'])

    def finish(self,step):
        tg.tioga_delete_()
