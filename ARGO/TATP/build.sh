#!/bin/bash -l

#############################
# Job comment
#############################
#SBATCH -N 1
#SBATCH -J TATPMake
#SBATCH -o make.out
#SBATCH -p cluster
#############################

make clean && make
