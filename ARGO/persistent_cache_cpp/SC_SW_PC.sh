#!/bin/bash -l

#############################
# Job comment
#############################
#SBATCH -N 4
#SBATCH -J PCSW
#SBATCH -o logSCN4p4_WEAK.out
#SBATCH -t 04:00:00
#############################
# OpenMPI Infiniband flags
#############################
OMPIFLAGS="--map-by ppr:4:node "
OMPIFLAGS+="--mca mpi_leave_pinned 1 "
OMPIFLAGS+="--mca btl_openib_allow_ib 1 "
OMPIFLAGS+="--mca btl openib,self,vader "

mpirun $OMPIFLAGS --bind-to socket ./pc_nvm
