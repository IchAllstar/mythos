# Copy to your local machine
# Put in correct parameter
# execute benchmark on remote machine (should use the benchmark branch with correct output format)
# start script to plot data 
# if problems, you can use the pyplot_generic.py script manually:
#   copy benchmark output to file and invoke pyplot_generic.py path/to/file.txt
PATH_TO_FILE_REMOTE="./Documents/mythos/kernel-knc/debug/0"
PATH_TO_PLOT="plot_generic.py"
USER="shertrampf"
HOST="lufn.informatik.tu-cottbus.de"
ssh $USER@$HOST "cat $PATH_TO_FILE_REMOTE" | sed "s,\x1B\[[0-9;]*[a-zA-Z],,g" > testfile.txt && python plot_generic.py testfile.txt 
