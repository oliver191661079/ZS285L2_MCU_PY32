#!/usr/bin/python
# hanchenfeng@actions-semi.com
# Sat, Jan 30, 2021  4:43:31 PM

import sys, getopt
import fileinput
import re
import os
from pathlib import Path
import csv

fstr=None
fmodel=None
count_files = 0
count_log = 0
count_log_s = 0
count_log_no_str = 0
count_log_translated = 0
list_str_id = []
list_model_id = []
MAX_ID_NAME_LEN = 20

function_dict = []

def print_debug(*args):
    #print(args)
    return

def print_to_file(line, end):
    print(line, end=end)

def get_function_name(filename, lineno):
    global function_dict

    f = "XXX"
    print_debug("To get function name.", filename, lineno)
    for func in function_dict :
        if (filename == func["file"]):
            if (lineno > func["line"]):
                f = func["fun"]
            else:
                break

    print_debug(f, lineno, filename)
    return f

def load_functions_table(name):
    global function_dict

    function_dict=[]
    i=0
    with open(name, newline='') as csvfile:
         spamreader = csv.reader(csvfile, delimiter='\t', quotechar='|')
         for row in spamreader:
             if row[0][0] != '!':
                if row[3] == "f":
                    function_dict.append({'fun':row[0], 'file':row[1], 'line':int(row[4][5:])})

    print_debug("Got fucntion dictionary.")
    function_dict.sort(key=lambda x: x.get('line'))
    print_debug(function_dict)

def str_to_id(filename, lineno, s):
    global list_str_id
    global fstr

    print_debug("String to id", filename, lineno, s)
    function=get_function_name(filename, lineno)
    s=s.replace("%p", "%08x")
    s=s.replace("%zu", "%u")
    s=s.replace("\\n", "")
    s=s.replace("\\r", "")
    str_for_id=s.replace(" ", "_")
    str_for_id=str_for_id.replace("\n", "")
    str_for_id=str_for_id.replace("\r", "")
    str_id=''.join(e for e in str_for_id if (e.isalnum() or e == '_'))
    str_id="ALF_STR_"+function+"__"+str_id[0:MAX_ID_NAME_LEN].upper()
    if(str_id not in list_str_id):
        list_str_id += [str_id]
        fstr.write(str_id + ", //\"[" + function + "] " + s[1:] + "\n")

    return str_id

def model_to_id(name):
    global fmodel 
    global list_model_id

    base=os.path.basename(name)
    model=os.path.splitext(base)[0]
    model=model.replace(" ", "_")
    model=model.replace("-", "_")
    model=''.join(e for e in model if (e.isalnum() or e == '_'))
    model_id="ALF_MODEL_" + model.upper()
    if(model_id not in list_model_id):
        list_model_id += [model_id]
        fmodel.write(model_id + ', //"' + model + '"\n')

    return model_id

def translate_line(mode, filename, line_no, line):
    global count_log
    global count_log_s
    global count_log_no_str
    global count_log_translated 
    global has_log 

    count_log += 1
    r=r'%s'
    m = re.search(r, line);
    if m is not None:
        count_log_s += 1
        return line;

    r=r'".*?"'
    m = re.search(r, line);
    if m is not None:
        print_debug("to translate line", mode, filename, line_no, line)
        strid = str_to_id(filename, line_no,  m.group())
    else:
        count_log_no_str += 1
        return line;

    count_log_translated += 1
    num = line[m.end():].count(',')
    if ( 0 == mode):
        line_start=line[:m.start()].replace("printk(", "ACT_LOG_ID_INF(") 
    else:
        line_start=line[:m.start()].replace("SYS_LOG_", "ACT_LOG_ID_") 
    has_log = True;

    return (line_start + strid + ", " + str(num) + line[m.end():])


def translate_log(name):
    global has_log 

    has_log = False
    has_header = False
    ifdef_meet = False
    line_last_include = 0
    with fileinput.FileInput(name, inplace=True) as file_object:
        for line in file_object:
            if (not has_header):
                r=r'#include.*sys_log\.h'
                m = re.search(r, line)
                if(m is not None):
                    has_header = True;
                    line_last_include = file_object.lineno()

            if (not has_header):
                if (not ifdef_meet):
                    r=r'#ifdef'
                    m = re.search(r, line)
                    if(m is not None):
                        ifdef_meet = True;

                if (not ifdef_meet):
                    r=r'#if define'
                    m = re.search(r, line)
                    if(m is not None):
                        ifdef_meet = True;

                '''
                if (not ifdef_meet):
                    r=r'#ifndef'
                    m = re.search(r, line)
                    if(m is not None):
                        ifdef_meet = True;
                '''

                if (not ifdef_meet):
                    r=r'#include'
                    m = re.search(r, line)
                    if(m is not None):
                        line_last_include = file_object.lineno()

            r=r'SYS_LOG.*(.*);'
            m = re.search(r, line)
            if(m is None):
                r=r'printk(.*);'
                m = re.search(r, line)
                if(m is None):
                    print_to_file(line, end='')
                else:
                    translated_line = translate_line(0, name, file_object.lineno(), line)
                    print_to_file(translated_line, end='')
            else:
                translated_line = translate_line(1, name, file_object.lineno(), line)
                print_to_file(translated_line, end='')
    file_object.close()

    #To insert header include and macro definition after last include, but not in condition macros.
    if (has_log):
        model_id=model_to_id(name)
        with fileinput.FileInput(name, inplace=True) as file_object:
            insert_position  = 0
            for line in file_object:
                if (file_object.lineno() == 1 and line_last_include == 0):
                    print_to_file("#include <logging/sys_log.h>", end='\n')
                    print_to_file("#define ACT_LOG_MODEL_ID " + model_id, end='\n'); 
                    print_to_file(line, end='')
                else:
                    if (file_object.lineno() ==  line_last_include):
                        print_to_file(line, end='')
                        if (not has_header):
                            print_to_file("#include <logging/sys_log.h>", end='\n')
                        print_to_file("#define ACT_LOG_MODEL_ID " + model_id, end='\n'); 
                    else:
                        print_to_file(line, end='')
        file_object.close()

def print_help():
    print('test.py -t <tag_file> -i <inputfile> | -d <directory> | -L <file_list>')
    print('test.py -h')
    print('tag_file: ctags -L <file_list> --fields=+n')

def main(argv):
    global fstr
    global fmodel
    global count_files
    global count_log
    global count_log_s
    global count_log_no_str
    global count_log_translated 

    count_files = 0 
    count_log = 0 
    count_log_s = 0 
    count_log_no_str = 0 
    count_log_translated = 0 
    list_str_id=[]
    list_model_id=[]

    inputfile = None
    directory = None
    inputlist = None
    tag_file  = None
    str_id_file = 'log_str_id.h'
    model_id_file = 'log_model_id.h'


    try:
        opts, args = getopt.getopt(argv,"ht:i:d:L:",["tag=","ifile=","directory=","list="])
    except getopt.GetoptError:
        print_help()
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            print_help()
            sys.exit(0)
        elif opt in ("-i", "--ifile"):
            inputfile = arg
        elif opt in ("-d", "--directory"):
            directory = arg
        elif opt in ("-L", "--list"):
            inputlist = arg
        elif opt in ("-t", "--tag"):
            tag_file = arg

    load_functions_table(tag_file)

    fstr = open(str_id_file, 'w')
    fmodel = open(model_id_file, 'w')
    if(inputfile is not None):
        print(inputfile)
        translate_log(inputfile);
        count_files += 1

    if(directory is not None):
        pathlist = Path(directory).glob('**/*.c')
        for path in pathlist:
            # because path is object not string
            path_in_str = str(path)
            print(path_in_str)
            translate_log(path_in_str)
            count_files += 1

    if(inputlist is not None):
        rd = open (inputlist, "r")
        # Read list of lines
        fileslist = rd.readlines()
        for path in fileslist:
            path_in_str = path.strip('\r\n')
            print(path_in_str)
            translate_log(path_in_str)
            count_files += 1


    fmodel.close()
    fstr.close()
    print("Files:", count_files, ", Logs:", count_log, count_log_translated, count_log_s, count_log_no_str)

if __name__ == "__main__":
    main(sys.argv[1:])
