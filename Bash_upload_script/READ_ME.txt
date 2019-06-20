

HOW TO USE THE SCRIPT:

1-Inside the upload.sh file you should:
	a-define the number of motes to be programmed in the for loop eg.5 motes->"for i in {1..5};"
	b-define the project .c file name on the command variable. eg. "c2x_v1.upload"
2-Copy the upload.sh file to the project folder
3-Configure another build entry eg."Build ALL" in Eclipse project with command "bash upload.sh"
4-Build the project with that command.


