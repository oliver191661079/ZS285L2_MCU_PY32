#!/usr/bin/env python3
#
# Build Actions SoC firmware (RAW/USB/OTA)
#
# Copyright (c) 2024 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#
import subprocess
import os
import sys
import requests
import base64
import json
from requests.auth import HTTPBasicAuth
from requests.auth import HTTPDigestAuth

import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

global server_url
server_url = 'https://harman-ten-actz-1/sign-firmware-production'  # server url

def get_sha256_hash(file_path):
    cmd = ['openssl', 'dgst', '-sha256', file_path]
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    stdout, stderr = process.communicate()

    if process.returncode != 0:
        raise subprocess.CalledProcessError(process.returncode, cmd, output=stdout, stderr=stderr)

    hash_str = stdout.strip().split('\n')[0]
    hash_value = hash_str.split('=')[1].strip()
    return hash_value

def send_file_for_signing(server_url, file_path, app_conf):
    try:
        hash_string = get_sha256_hash(file_path)
        #print(hash_string)

        headers = {
            "Content-type": "application/json"
        }

        if app_conf == 'flip7' or app_conf == 'local_music':
            data = {
                "hashOfFirmwareImage": hash_string,
                "productModelName": "JBL Flip 7"
            }
        elif app_conf == 'charge6' or app_conf == 'TrailProGo' or app_conf == 'partyband':
            data = {
                "hashOfFirmwareImage": hash_string,
                "productModelName": "JBL Charge 6"
            }
        elif app_conf == 'grip':
            data = {
                "hashOfFirmwareImage": hash_string,
                "productModelName": "JBL Grip"
            }
        elif app_conf == 'SS5':
            data = {
                "hashOfFirmwareImage": hash_string,
                "productModelName": "HK SoundSticks 5"
            }
        elif app_conf == '1300mk2':
            data = {
                "hashOfFirmwareImage": hash_string,
                "productModelName": "JBL Bar 1300MK2 SOD"
            }
        elif app_conf == 'AS5':
            data = {
                "hashOfFirmwareImage": hash_string,
                "productModelName": "HK Aura Studio 5"
            }
        elif app_conf == 'bb4':
            data = {
                "hashOfFirmwareImage": hash_string,
                "productModelName": "JBL Boombox 4"
            }
        elif app_conf == 'BandBox_Solo':
            data = {
                "hashOfFirmwareImage": hash_string,
                "productModelName": "JBL Solo"
            }
        elif app_conf == 'BandBox_Trio':
            data = {
                "hashOfFirmwareImage": hash_string,
                "productModelName": "JBL Trio"
            }
        else:
            raise Exception("An error occurred while sign for app_conf: %s" %(app_conf))
            return 1
        #print(data)

        response = requests.post(server_url, auth=('admin', 'PsB4Pak3AIDM6c3kRJQyqgAzptMLCRnv'),
                             data=json.dumps(data), headers=headers, verify=False)

        if response.status_code == 200:
            #print("request ok\n")
            try:
                response_data = response.json()

                if 'signature' in response_data:
                    #print("sign data: ", response_data['signature'])
                    signature = response_data['signature']
                    binary_data = base64.b64decode(signature)
                    return binary_data
                else:
                    print("error: no signature key word in response")
            except json.JSONDecodeError:
                print("error: not jason data")
        else:
            print("request failed, status code: ", response.status_code)

        return None
    except requests.RequestException as e:
        print("An error occurred while sending data for signing: %s" % e)
        return None

def sign_file(file_path, output_path, app_conf):
	#if os.path.exists(output_path):
    #    print(f"sign file %s existed" %(output_path))
    #    return

    signature = send_file_for_signing(server_url, file_path, app_conf)

    if signature is not None:
        with open(output_path, 'wb') as signature_file:
            signature_file.write(signature)
            return 0
    return 1


def verify_signature(public_key_path, file_path, signature_path):
    try:
        if not os.path.exists(public_key_path):
            print("key file %s not existed" % public_key_path)
            return

        with open(file_path, 'rb') as file_to_verify, open(signature_path, 'rb') as signature_file:
            cmd = ['openssl', 'dgst', '-sha256', '-verify', public_key_path, '-signature', signature_path]
            process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            stdout, stderr = process.communicate(file_to_verify.read())

        if process.returncode != 0:
            print("Signature of %s is invalid." % (file_path))
        else:
            print("Signature of %s is valid." % (file_path))
        return process.returncode
    except Exception as e:
        raise Exception("An error occurred while verifying the signature: %s" % e)
        return 1

def merge_app_file(original_file, cert_file, file_to_sign):
    with open(original_file, 'rb') as file1:
        data1 = file1.read()

    with open(cert_file, 'rb') as file2:
        data2 = file2.read()

    with open(file_to_sign, 'wb') as merged_file:
        merged_file.write(data1)
        merged_file.write(data2)

def rsa_sign_app_file(original_file, cert_file, file_to_sign, key_path, app_conf):
    return_code = sign_file(original_file, cert_file, app_conf)
    if return_code != 0:
        sys.exit(return_code)

    return_code = verify_signature(key_path, original_file, cert_file)
    if return_code != 0:
        sys.exit(return_code)

    merge_app_file(original_file, cert_file, file_to_sign)
    sys.exit(0)

def main(argv):
    rsa_sign_app_file(argv[1], argv[2], argv[3], argv[4], argv[5])

if __name__ == "__main__":
    main(sys.argv)