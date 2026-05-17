#!/bin/bash

# Make all the files in MIDAS_DATA group writeable
# chmod -R g+rw $MIDAS_DATA # for now leave while determine running modes

# Ensure MIDAS run is stopped
echo "Stopping MIDAS run..."
odbedit -q -c "stop now"

# Make sure all ODB processes have stopped i.e. mhttpd, mlogger (this will kill the screens too..)
odbedit -q -c "sh all"

# Cleanup hanging ODB clinets
echo "Cleaning ODB..."
odbedit -q -c "cleanup -f"

# Get and print current run number
echo "Getting run number..."
odbRun=$(odbedit -q -c "ls -v 'Runinfo/Run number'")
echo "Run number in ODB = $odbRun"
# Check ODB run number against last saved json file
jsonLast=$(ls -dvr $MIDAS_DATA/*.json | head -1)
jsonRun=$(echo ${jsonLast: -10: -5} | sed 's/^0*//')
echo "Last json file with a run number = $jsonLast and run = $jsonRun"
jsonRunX=`grep -i "Runinfo/Run number" $MIDAS_DATA/last.json | head -1 | awk '{print $13}' | cut -d, -f1`
echo "Run number in last.json = $jsonRunX"

# Start mhttpd in a screen
screen -S mhttpd -X quit # should do nothing and issue warning since odb command above has killed them
screen -dmS mhttpd bash -c 'source $MANCX_USER_DIR/setup.sh; mhttpd;'
#

# NEW: -> sleep
sleep 2
#

# NEW -> Start frontend
screen -S caen -X quit   # Kill any old caen session
#screen -dmS caen bash -c "source $MANCX_USER_DIR/setup.sh; $MANCX_USER_DIR/midas/build/caen_dig2"

screen -S daq -X quit   # Kill any old caen session
screen -dmS daq bash -c "source $MANCX_USER_DIR/setup.sh; $MANCX_USER_DIR/cpp/build/main"

screen -S housekeeping -X quit   # Kill any old session
screen -dmS housekeeping bash -c "source $MANCX_USER_DIR/setup.sh; python $MANCX_USER_DIR/cpp/housekeeping/main.py"

screen -S daq-webserver -X quit   # Kill any old session
screen -dmS daq-webserver bash -c "source $MANCX_USER_DIR/setup.sh; python $MANCX_USER_DIR/cpp/webserver/main.py"
#

# Start mlogger/Logger in a screen
screen -S mlogger -X quit # should do nothing...
screen -dmS mlogger bash -c 'source $MANCX_USER_DIR/setup.sh; mlogger;'
#
