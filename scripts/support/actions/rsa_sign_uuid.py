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

def get_sha256_hash(uuid_str):
    hash_cmd = hashlib.sha256()
    hash_cmd.update(uuid_str.encode(encoding='UTF-8'))
    return hash_cmd.hexdigest()

def send_file_for_signing(server_url, uuid_str):
    try:
        hash_string = get_sha256_hash(uuid_str)
        #print(hash_string)

        headers = {
            "Content-type": "application/json"
        }

        data = {
            "hashOfFirmwareImage": hash_string,
            "productModelName": "JBL Flip 7"
        }

        print(data)

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

def sign_uuid(uuid_str):
	#if os.path.exists(output_path):
    #    print(f"sign file %s existed" %(output_path))
    #    return

    signature = send_file_for_signing(server_url, uuid_str)
    print(binascii.hexlify(signature))
    with open("uuid_cert.bin", 'wb') as file:
        file.write(signature)

def main(argv):
    sign_uuid(argv[1])

if __name__ == "__main__":
    main(sys.argv)