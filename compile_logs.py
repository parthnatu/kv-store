import sys
import os
import os.path as path
import glob

def main(argv):
    print (argv)
    n_clients = int(argv[0])
    log_dir = argv[1]
    
    logs_all = {}

    for i in range(n_clients):
        with open(log_dir + "/clientlogs_" + str(i) + ".txt", "r") as f:
            for line in f.readlines():
                splits = line.strip().split('\t')
                timestamp = float(splits[0])
                log = splits[1]
                if timestamp not in logs_all:
                    logs_all[timestamp] = log
                else:
                    print ("Fak it")

    logs = []

    for t in sorted(logs_all.keys()):
        logs.append(logs_all[t] + "\n")

    with open(log_dir + "/compiled_log.txt", "w") as f:
        f.writelines(logs)

if __name__ == "__main__":
   main(sys.argv[1:])