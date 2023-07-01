#!/bin/bash

# Check if the CETI app is running
if pgrep -x "cetiTagApp" >/dev/null
then
    # Get the location of this script, since will want to
    #  use the pipes in that directory rather than the calling directory.
    IPC_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

    # Send a command to the ceti app
    echo "SENDING : $1"
    if echo $1 > $IPC_DIR/cetiCommand
    then
      # Read and print the response
      echo "RESPONSE:"
      cat $IPC_DIR/cetiResponse
    else
      echo "Error sending command - make sure this script is running as root"
    fi
else
    echo "The CETI tag app is not running."
fi


