#!/usr/bin/env python3
#
# UART OTA client
#
# Copyright (c) 2024 Actionstech Co., Ltd
#
#
import serial
import time
import sys
import getopt
from zlib import crc32
from struct import unpack

#Select log level in {0,1,2}: 0-Error/Warning , 1-debug, 2-verbose debug
CONFIG_LOG_LEVEL=1
CONFIG_LOG_TO_FILE=False

def app_log(str, level=0):
    if(level <= CONFIG_LOG_LEVEL):
        print(str, file=log_file_obj)

def serial_read(ser, size=None):
    """\
    Read until a termination sequence is found ('\n' by default), the size
    is exceeded or until timeout occurs.
    """
    line = b""
    while True:
        c = ser.read(1)
        if c:
            line += c
            if size is not None and len(line) >= size:
                break

    app_log("\nRead: {:s}".format(line.hex()), 1)
    return line

def serial_write(ser, data):
    app_log("\nWrite: {:s}".format(data.hex()), 2)
    ser.write(data)

def ota_shake_hand(ser):
    shakehand1 = bytes("OK?".encode("ascii"))
    shakehand2 = bytes("YES,YOU?".encode("ascii"))
    shakehand3 = bytes("METOO".encode("ascii"))
    step = 0

    app_log("handshake step {:d}".format(step))

    while True:
        if step == 0:
            if ser.inWaiting():
                data = serial_read(ser, size=3)
                if data == shakehand1:
                    ser.flushInput()
                    serial_write(ser, shakehand2)
                    step = 1
                    app_log("handshake step {:d}".format(step))

        if step == 1:
            if ser.inWaiting():
                data = serial_read(ser, size=5)
                if data == shakehand3:
                    step = 2
                    app_log("handshake step {:d}".format(step))
                    break
        
        time.sleep(0.01)

    return step


def ota_command_handler(ser, ota_data, data):
    ret = 0
    try:
        command = data[0:8].decode("ascii")
        seq = unpack("<I", data[8:12])[0]
        offset = unpack("<I", data[12:16])[0]
        len = unpack("<I", data[16:20])[0]
    except:
        app_log("unpack exception")
    else:
        app_log("Command: " + command + " {:08d} {:08x} {:08x}".format(seq, offset, len), 1)
        if command == "REPORT01":
            if(offset):
                ret = 1
            else:
                ret = -1
        if command == "PULLDATA":
            data_to_write = ota_data[offset:offset + len]
            crc = crc32(data_to_write)
            app_log("\noutput: crc={:08x}, seq={:08x}".format(crc,seq), 1)
            serial_write(ser, crc.to_bytes(length=4, byteorder="little"))
            serial_write(ser, seq.to_bytes(length=4, byteorder="little"))
            serial_write(ser, data_to_write)
    return ret

def ota_process(ser, ota_data):
    done = 0
    while (not done):
        if ser.inWaiting():
            data = serial_read(ser, size=20)
            try:
                seq = unpack("<I", data[8:12])[0]
                offset = unpack("<I", data[12:16])[0]
                len = unpack("<I", data[16:20])[0]
            except:
                app_log("unpack exception")
            else:
                done = ota_command_handler(ser, ota_data, data)
        else:
            time.sleep(0.001)
    return done 

def main():
    #Default parameters.
    global log_file_obj
    if CONFIG_LOG_TO_FILE:
        print("log to log.txt")
        log_file_obj = open("log.txt", "w+")
    else:
        log_file_obj = sys.stdout

    input_file = "ota.bin"
    com_port = "/dev/ttyS19"
    com_baud_rate = 3000000

    #Get parameters from command line.
    opts, args = getopt.getopt(sys.argv[1:], "hi:c:b:")
    for op, value in opts:
        if op == "-h":
            print("uart_ota.py [-c COMPORT] [-b baudrate] [-i ota_file] [-h]")
            exit(0)
        if op == "-i":
            input_file = value
        if op == "-c":
            com_port = value
        if op == "-b":
            com_baud_rate = value

    #Print parameters.
    app_log("ota file: " + input_file)
    app_log("com_port: " + com_port)
    app_log("com_baud_rate: {0}".format(com_baud_rate))
    app_log("----------------------------")

    #Open serial
    try:
        ser = serial.Serial(com_port, com_baud_rate, timeout=10000000)
    except Exception as e:
        app_log(str(e))
        exit(-1)

    with open(input_file, "rb") as ota_file:
        ota_file_data = ota_file.read()
        app_log("{0} file size: {1} ".format(input_file, len(ota_file_data)))
        ota_shake_hand(ser)
        ret = ota_process(ser, ota_file_data)
        if (ret == 1):
            app_log("Ota done successfully.")
        else:
            app_log("Ota fails!")

    if(CONFIG_LOG_TO_FILE):
        print("exit")
        log_file_obj.flush()
        log_file_obj.close()


if __name__=="__main__":
    main()
