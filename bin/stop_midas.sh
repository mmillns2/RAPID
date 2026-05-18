#!/bin/bash

screen -S caen -X quit
screen -S daq -X quit
screen -S housekeeping -X quit
screen -S daq-webserver -X quit
screen -S mhttpd -X quit
screen -S mlogger -X quit
