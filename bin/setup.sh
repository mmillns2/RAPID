#!/bin/bash

# Get the location of this script irrespective of where it is run from and
# whether it is run as ./setup.sh ./xxx/setup.sh, source setup.sh, source xxx/setup.sh etc
#
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do
  DIR="$( cd -P "$( dirname -- "$SOURCE" )" >/dev/null 2>&1 && pwd )"
  TARGET="$(readlink -- "$SOURCE")"
  [[ "$TARGET" != /* ]] && SOURCE="$DIR/$TARGET" || SOURCE="$TARGET"
done
export MANCX_USER_DIR="$( cd -P "$( dirname -- "$SOURCE" )" >/dev/null 2>&1 && pwd )"

if [ "$HOSTNAME" = "mu2edaq2.blackett.manchester.ac.uk" ]
then
    export MANCX_HOME=/work/mancx/packages
    export CAEN_FE=$MANCX_HOME/caen_felib-v1.3.2
    export CAEN_DIGI=$MANCX_HOME/caen_dig1-v1.1.4
    export CAEN_DIG2=$MANCX_HOME/caen_dig2-v1.8.1
    export ROOT_HOME=$MANCX_HOME/root
    export PYTHON_HOME=$MANCX_HOME/python
    export MIDASSYS=/work/mancx/packages/midas
    export MIDAS_EXPTAB=$MANCX_HOME/midas/online/exptab
    export MIDAS_DATA=$MANCX_HOME/midas/data
    OK=1
else
    echo "ERROR in setup.sh - $HOSTNAME not recgonised.."
    OK=0
fi

if [ "$OK" -eq "1" ]
then
    . $ROOT_HOME/bin/thisroot.sh # setup root
    . $PYTHON_HOME/bin/activate  # setup python3 virtual environment
    export MIDAS_EXPT_NAME=MANCX # name of MIDAS instance
    export CAEN_FE_LIBS=$CAEN_FE/lib
    export CAEN_DIGI_LIBS=$CAEN_DIGI/lib
    export CAEN_DIG2_LIBS=$CAEN_DIG2/lib
    
    export PATH=$PATH:$MIDASSYS/bin:$MANCX_USER_DIR/bin
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$MIDASSYS/lib:$CAEN_FE_LIBS:$CAEN_DIGI_LIBS:$CAEN_DIG2_LIBS
    
    echo "MANCX User Directory --- "$MANCX_USER_DIR
    echo "ROOTSYS ---------------- "$ROOTSYS
    echo "PYTHON ----------------- "$VIRTUAL_ENV
    echo "MIDASSYS --------------- "$MIDASSYS
    echo "MIDAS_DATA ------------- "$MIDAS_DATA
fi

