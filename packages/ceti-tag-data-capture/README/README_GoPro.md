### Should be updated later ###

This readme file provides a brief introduction on goPro system.


## Requirement
Install python bluetooth modules to use goPros. 
Python version >=3.8. We used version 3.9 for this setting. The pip install should be run using "sudo".

```
cd 
git clone https://github.com/gopro/OpenGoPro.git
cd OpenGoPro/demos/python/tutorial/
sudo pip install -e .
sudo pip install bleak
```

## Building
### 1. Clone the github repo.
```
cd
git clone https://github.com/Project-CETI/whale-tag-embedded.git
cd whale-tag-embedded
git fetch origin
git checkout -b v2_2_refactor_goPros remotes/origin/v2_2_refactor_goPros
```
### 2. Prepare to run the code.
   
Make the data folder that stores data.
```
sudo mkdir -p /data
sudo chmod 777 /data
```

Make the command files that control the operation.
```
cd ~/whale-tag-embedded/packages/ceti-tag-data-capture/ipc/
touch cetiCommand
touch cetiResponse
```
### 3. Compile the c code.

Compile the c code as below.
``` 
cd ~/whale-tag-embedded/packages/ceti-tag-data-capture
(find src/ -name "*.o" -type f -delete); (rm bin/cetiTagApp); make
(cd bin; sudo ./cetiTagApp)
```


## Run & quit

Run the code by using following commands.

```
cd ~/whale-tag-embedded/packages/ceti-tag-data-capture
(cd bin; sudo ./cetiTagApp)
```

Quit the code by using following commands in the another terminal.
```
cd ~/whale-tag-embedded/packages/ceti-tag-data-capture/ipc/
echo "quit" > cetiCommand
```

## Check the result

Then it should send commands to start the GoPros! It will also log command times in /data/data_goPros.csv so we know when the cameras started and stopped.

To quit the main tag program, open the file ~/whale-tag-embedded/packages/ceti-tag-data-capture/ipc/cetCommand, enter quit, and save the file. It should send commands to stop the GoPros, then exit the program.  Then Ctrl+C the Python program.

## List of GoPro cameras we have
1. GoPro6059
D9:49:D9:22:35:4E
2. GoPro7192
CB:08:AB:72:92:C9

## Things to check or to do
1. Should check other bluetooth related settings.
