#!/usr/bin/env python

# Copyright (C) 2011 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import getopt # would rather use argparse, but that depends on python 2.7
import sys
import os
import math
import tempfile
import subprocess
from string import Template

good_lfstk_versions = ["1.8.6"]

def usage():
    print """
Usage:
lfstk_wrapper.py  <options> images...

-h | --help                 Print this message
-g | --stepping [C0|B0]     Penwell stepping (required)
-t | --tmpdir [path]        Directory for temp files (default /tmp)
-s | --signed               Sign the images (default unsigned)
-k | --keydir [path]        Directory for signing key files (required)
-o | --output [filename]    Output filename for stitched image (required)
-l | --lfstk [path]         Path to lfstk executable (default look in PATH)

OS images properly end with .bin
2nd Stage firmware images end with .fv
Other image types not yet supported.
Images are written in the order they are provided on the command line
"""

def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hg:t:sk:o:l:",
                ["help", "stepping=", "tmpdir=", "signed", "keydir=", "output=",
                    "lfstk="])
    except getopt.GetoptError, err:
        print str(err)
        usage()
        sys.exit(1)

    signed = False
    outfile = None
    keydir = None
    tmpdir = "/tmp"
    stepping = None
    images = args
    lfstk_path = "lfstk"

    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit()
        elif o in ("-g", "--stepping"):
            stepping = a
        elif o in ("-t", "--tmpdir"):
            tmpdir = a
        elif o in ("-s", "--signed"):
            signed = True
        elif o in ("-k", "--keydir"):
            keydir = a
        elif o in ("-o", "--output"):
            outfile = a
        elif o in ("-l", "--lfstk"):
            lfstk_path = a
        else:
            assert False, "unhandled option"

    # Sanity check what was passed in
    bad_params = False
    if not stepping:
        print >>sys.stderr, "You must specify board stepping with --stepping!"
        bad_params = True
    if not keydir:
        print >>sys.stderr, "You must specify a valid signing key files directory with --keydir"
        bad_params = True
    if not os.path.isdir(keydir):
        print >>sys.stderr, "Invalid key files directory:", keydir
        bad_params = True
    if not outfile:
        print >>sys.stderr, "You must specify an output file with --output"
        bad_params = True
    if not images:
        print >>sys.stderr, "You must specify at least one image file"
        bad_params = True
    for image in images:
        if not os.path.isfile(image):
            print >>sys.stderr, "Bad image filename", image
            bad_params = True
    if bad_params:
        sys.exit(1)

    ret = stitch_images(images, signed, stepping, keydir, outfile, lfstk_path, tmpdir)
    sys.exit(ret)


def stitch_images(images, signed, stepping, keydir, outfile,
            lfstk_path="lfstk", tmpdir="/tmp"):
    # Make sure we're using a good LFSTK version
    if get_lfstk_version(lfstk_path) not in good_lfstk_versions:
        print >>sys.stderr, "You have an unsupported version of LFSTK. Please use one of: " + `good_lfstk_versions`
        return 1

    # LFSTK doesn't know about D0, but C0 works fine for those boards
    if stepping not in ["B0","C0"]:
        stepping = "C0"

    penwell_xml, stitch_xml, override_txt = build_config_files(images, signed,
            stepping, keydir, outfile, tmpdir)

    if not run_lfstk(lfstk_path, penwell_xml, stitch_xml, override_txt):
        print >>sys.stderr, "LFSTK invocation FAILED!"
        return 2

    return 0


def write_tmp_file(file_suffix, tmpdir, body):
    # Automatically deleted on exit or close
    f = tempfile.NamedTemporaryFile(suffix=file_suffix, dir=tmpdir)
    #f = open(tmpdir+"/"+file_suffix, "wb") # uncomment to keep temp files
    f.write(body)
    f.flush()
    return f

def build_config_files(images, is_signed, stepping, key_dir, output_filename,
                        tmpdir):
    penwell_xml = write_tmp_file("penwell.xml", tmpdir,
            get_penwell_xml(images, is_signed, stepping))
    stitch_xml = write_tmp_file("stitch.xml", tmpdir,
            get_stitch_config(stepping))
    override_txt = write_tmp_file("override.txt", tmpdir,
            get_override(stepping, key_dir, output_filename))
    return (penwell_xml, stitch_xml, override_txt)

def get_lfstk_version(lfstk_path):
    try:
        output = subprocess.Popen([lfstk_path,"-V"], stdout=subprocess.PIPE).communicate()[0]
    except Exception, e:
        raise Exception("Bad or missing LFSTK binary")
    return output.strip().split()[1]

def run_lfstk(lfstk_path, penwell_xml, stitch_xml, override_txt):
    retval = subprocess.call([
                lfstk_path,
                '-l', stitch_xml.name,
                '-k', penwell_xml.name,
                '-o', override_txt.name])
    print
    penwell_xml.close()
    stitch_xml.close()
    override_txt.close()

    return retval == 0

# substitute key_dir
C0_keys_lines = """
Public Key0 = $key_dir/C0_0_public.key
Private Key0 = $key_dir/C0_0_private.key
Public Key1 = $key_dir/CRAK_1_public.key
Private Key1 = $key_dir/C0_234_private.key
Public Key2 = $key_dir/C0_234_public.key
Private Key2 = $key_dir/C0_234_private.key
Public Key3 = $key_dir/C0_234_public.key
Private Key3 = $key_dir/C0_234_private.key
Public Key4 = $key_dir/C0_234_public.key
Private Key4 = $key_dir/C0_234_private.key
"""

# substitute key_dir
B0_keys_lines = """
Public Key0 = $key_dir/empty.key
Private Key0 = $key_dir/empty.key
Public Key1 = $key_dir/empty.key
Private Key1 = $key_dir/empty.key
Public Key2 = $key_dir/public_primary_n2.0.3.key
Private Key2 = $key_dir/private_primary_d2.0.3.key
Public Key3 = $key_dir/public_primary_n2.0.3.key
Private Key3 = $key_dir/private_primary_d2.0.3.key
Public Key4 = $key_dir/public_primary_n2.0.3.key
Private Key4 = $key_dir/private_primary_d2.0.3.key
"""

key_lines_dict = {
        "B0" : B0_keys_lines,
        "C0" : C0_keys_lines
    }


# substitute keys_lines, output_filename
override_template = """
' Penwell C0 Example Overide file, use the -G option to stitch for C0 CRAK
' This file overides settings in the stitching.xml file.
'-------- Settings Section
ImageType = OSUSB
'ImageName is the output filename
ImageName = $output_filename
'
'-------- Files-to-Overide Section
SCU uCode = ./FW_Components/SCURuntime_C0.02.bin
Punit uCode = ./FW_Components/PNWMicrocode-mc0-v2.07-11ww18a.bin
x86 FW Boot = ./FW_Components/IA32FW_v00.3A.bin
Spectra-FTL = ./FW_Components/IA32FWSupp_v00.3A.bin
Validation Hooks = ./FW_Components/ValidationHooks_release_v00.4C_PNW_B0C0.bin

'-------- These are required if SignDnX is invoked
'DnxFile_ToSign = ./dnx_file.bin
'DnxFile_Signed = ./signed_dnx_output_file.bin

'-------- Penwell C0 Required Keys: Key selection is based on SMIP entries
'-------- Index reference
'0: SMIP, primary chaabi fw
'1: SCU + Punit
'2: IA fw,  chaabi ext fw, OS
'3: Open (ia fw, chaabi ext fw, OS)
'4: Open (ia fw, chaabi ext fw, OS)
'--------
$key_lines

'------- Signed FW Inputs to generate final IFWI  : -G
Intel Signed FW = ./FW_Components/ScuPunit_Signed.bin
'SIGNED_MIP_BIN = ./Signed_B0_MIP.bin

'        If there is no patch,comment out SIGNED_PATCH_BIN
'        If no Parameter block, comment out Patch_Parameter.
'SIGNED_PATCH_BIN = ./Signed_C0_Patch2.bin
'Patch_Parameter = ./patch_keyparam.bin

'------- Unsigned Outputs (to be sent to signing server) : -M, -P
MIP_BIN = ./output-files/MIP.bin

'    Patching Related
'         UPATCH_BIN is the raw SCU ROM patch input to -P
UPATCH_BIN = ./unsigned_scu_patch.bin
'         UPATCH_OUT is the unsigned Patch with patch header and
'         param block (output of -P); this is sent to the server
UPATCH_OUT = ./output-files/unsigned_patch.bin

'------- Security Firmware (Optional, comment out if not included in IFWI)
ICache_Image = ./FW_Components/Signed_iCache_C0_v00800.bin
Resident_Image = ./FW_Components/Signed_Resident_C0_v00800.bin
Extended_FW = ./FW_Components/Signed_extFW_C0_v00830.bin

'-------- end of Files-to-Overide Section
' End of file
"""

def get_override(stepping, key_dir, output_filename):
    sub_dict = {
            "stepping" : stepping,
            "key_dir" : key_dir,
            "output_filename" : output_filename
        }
    sub_dict["key_lines"] = Template(key_lines_dict[stepping]).substitute(sub_dict)
    return Template(override_template).substitute(sub_dict)

# substitute stepping
stitch_config_xml_template = """<?xml version="1.0" encoding="utf-8"?>
<MTKAutoStitchConfiguration xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Platform validValues="MSTN,MFDA0,MFDB0,MFDC0">
    <Value>MFD$stepping</Value>
  </Platform>
  <Path>
    <Name>Primary Public Key</Name>
    <Value>./sample-image-files/PublicKey.key</Value>
  </Path>
  <Path>
    <Name>Primary Private Key</Name>
    <Value>./sample-image-files/PrivateKey.key</Value>
  </Path>
  <Path>
    <Name>OS Public Key</Name>
    <Value>./sample-image-files/PublicKey.key</Value>
  </Path>
  <Path>
    <Name>OS Private Key</Name>
    <Value>./sample-image-files/PrivateKey.key</Value>
  </Path>
  <Path>
    <Name>SCU uCode</Name>
    <Value>./sample-image-files/SCU UCode.bin</Value>
  </Path>
  <Path>
    <Name>Punit uCode</Name>
    <Value>./sample-image-files/P-Unit.bin</Value>
  </Path>
  <Path>
    <Name>x86 FW Boot</Name>
    <Value>./sample-image-files/x86.bin</Value>
  </Path>
  <Path>
    <Name>Spectra-FTL</Name>
    <Value>./sample-image-files/SpectraFTL.bin</Value>
  </Path>
  <Path>
    <Name>Validation Hooks</Name>
    <Value>./sample-image-files/ValidationHooks.bin</Value>
  </Path>
  <ImageType validValues="FWUSB,FWSPI,FWNAND,FWSLE,OSUSB,OSSPI,OSNAND,OSSLE">
    <Value>OSUSB</Value>
  </ImageType>
  <ImageName>
    <Value>./PA0_fwusb.bin</Value>
  </ImageName>
</MTKAutoStitchConfiguration>
"""

def get_stitch_config(stepping):
    return Template(stitch_config_xml_template).substitute({"stepping":stepping})


# substitute attributes signed image_size filename dest_ptr handoff_ptr src_ptr
os_image_xml_template = """<os_image>
<minor_revision>0</minor_revision>
<major_revision>0</major_revision>
<source_pointer>$src_ptr</source_pointer>
<source_pointer_usb>$src_ptr</source_pointer_usb>
<source_pointer_nand>34</source_pointer_nand>
<destination_pointer>$dest_ptr</destination_pointer>
<handoff_pointer>$handoff_ptr</handoff_pointer>
<image_filepath>$filename</image_filepath>
<image_size>$image_size</image_size>
<intel_reserved>0</intel_reserved>
<image_attributes>$attributes</image_attributes>
<is_image_signed>$signed</is_image_signed>
<is_mtk_processed>$signed</is_mtk_processed>
<is_partition>0</is_partition>
<partition_status>0</partition_status>
<partition_type>0</partition_type>
</os_image>
"""

attributes_dict = {
        "bin" : (0, 1),
        "fv"  : (8, 9),
	"img" : (None, 3), # Filesystem image, always unsigned
    }

def get_one_os_image_xml(filename, is_signed, offset):
    filesize_sectors = int(math.ceil(os.path.getsize(filename) / 512.0))
    extension = filename.rsplit(".", 1)[1].lower()

    is_os = (extension == "bin")

    if (extension == "img"):
        is_signed = False

    if is_signed:
        attr = attributes_dict[extension][0]
    else:
        attr = attributes_dict[extension][1]

    sub_dict = {
            "signed"        : "1" if is_signed else "0",
            "image_size"    : str(filesize_sectors + 1) if is_signed else filesize_sectors,
            "filename"      : filename,
            "attributes"    : str(attr),
            "dest_ptr"      : str(0x01100000) if is_os else str(0),
            "handoff_ptr"   : str(0x01101000) if is_os else str(0),
            "src_ptr"       : str(offset + 1)
        }
    return (offset + filesize_sectors, Template(os_image_xml_template).substitute(sub_dict))

def get_os_image_xml(images, is_signed):
    offset = 0
    output_xml = ""

    for imagefile in images:
        offset, xml_chunk = get_one_os_image_xml(imagefile, is_signed, offset)
        output_xml = output_xml + xml_chunk

    return output_xml


# substitute stepping osimage_lines num_images
penwell_xml_template = """<?xml version="1.0" encoding="utf-8"?>
   <platform PlatformName="penwell" UMGFWTKVersion="2.2.7" Step="$stepping" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="Medfield_B0_Schema.xsd">
 <Panel header="SMIP" title="Signed MIP" start_offset="0000" end_offset="000B" />
 <Panel header="SMIP" title="Misc SMIP Fields" start_offset="000C" end_offset="001B" />
 <Panel header="SMIP" title="ISPV" start_offset="001C" end_offset="001C" class_type="UMGFWTK.Modules.Isps.IspsViewer" />
 <Panel header="SMIP" title="MSIC Registers" start_offset="002C" end_offset="002F" />
 <Panel header="SMIP" title="MSIC Charger Registers" start_offset="0030" end_offset="003F" />
 <Panel header="SMIP" title="MSIC VR Overrides" start_offset="0040" end_offset="0043" />
 <Panel header="SMIP" title="Validation Flags Register" start_offset="0044" end_offset="0047" />
 <Panel header="SMIP" title="FHOB" start_offset="0048" end_offset="0057" />
 <Panel header="SMIP" title="Voltage Rail Delay Table" start_offset="0058" end_offset="0077" />
 <Panel header="SMIP" title="Debug Profile Masks" start_offset="0078" end_offset="0097" />
 <Panel header="SMIP" title="IMR Registers" start_offset="0098" end_offset="0117" />
 <Panel header="SMIP" title="GCSB - Penwell GPIO Configuration Set Bits" start_offset="0118" end_offset="015F" />
 <Panel header="SMIP" title="GCCB - Penwell GPIO Configuration Clear Bits" start_offset="0160" end_offset="01A7" />
 <Panel header="SMIP" title="MSIC VR Registers" start_offset="01A8" end_offset="01C7" />
 <Panel header="SMIP" title="Received Delays" start_offset="01C8" end_offset="01D3" />
 <Panel header="SMIP" title="GP Pin Programming Header" start_offset="01D4" end_offset="0295" />
 <Panel header="SMIP" title="Intel Reserved 0x0296" start_offset="0296" end_offset="029B" />
 <Panel header="SMIP" title="Key Index for IA Hash Verification" start_offset="029C" end_offset="029F" />
 <Panel header="SMIP" title="USB Descriptor Overrides" start_offset="02A0" end_offset="02BF" />
 <Panel header="SMIP" title="IO Slew Rates Initialized in FLIS" start_offset="02C0" end_offset="02CB" />
 <Panel header="SMIP" title="Intel Reserved 0x02CC" start_offset="02CC" end_offset="02CF" />
 <Panel header="SMIP" title="Security FW Sizes" start_offset="02D0" end_offset="02DF" />
 <Panel header="SMIP" title="RTC Config Register" start_offset="02E0" end_offset="02E3" />
 <Panel header="SMIP" title="Battery Settings" start_offset="02E4" end_offset="02E7" />
 <Panel header="SMIP" title="VAUDA Configuration for MSIC Rails" start_offset="02E8" end_offset="02EB" />
 <Panel header="SMIP" title="USB Product Name" start_offset="02EC" end_offset="02F3" />
 <Panel header="SMIP" title="USB Product Manufacturer" start_offset="02F4" end_offset="02FB" />
 <Panel header="SMIP" title="USB Product Serial" start_offset="02FC" end_offset="030B" />
 <Panel header="SMIP" title="PTI Field" start_offset="030C" end_offset="030F" />
 <Panel header="SMIP" title="Intel Reserved 0x0310" start_offset="0310" end_offset="0313" />
 <Panel header="SMIP" title="SBCT Supportted Battery Characteristics Table 1" start_offset="0314" end_offset="0326" />
 <Panel header="SMIP" title="Battery 1 Header" start_offset="0327" end_offset="0331" />
 <Panel header="SMIP" title="Battery 1 Temp Range 4" start_offset="0332" end_offset="033E" />
 <Panel header="SMIP" title="Battery 1 Temp Range 3" start_offset="033F" end_offset="034B" />
 <Panel header="SMIP" title="Battery 1 Temp Range 2" start_offset="034C" end_offset="0358" />
 <Panel header="SMIP" title="Battery 1 Temp Range 1" start_offset="0359" end_offset="0367" />
 <Panel header="SMIP" title="Battery 2 Header" start_offset="0368" end_offset="0372" />
 <Panel header="SMIP" title="Battery 2 Temp Range 4" start_offset="0373" end_offset="037F" />
 <Panel header="SMIP" title="Battery 2 Temp Range 3" start_offset="0380" end_offset="038C" />
 <Panel header="SMIP" title="Battery 2 Temp Range 2" start_offset="038D" end_offset="0399" />
 <Panel header="SMIP" title="Battery 2 Temp Range 1" start_offset="039A" end_offset="03A8" />
 <Panel header="SMIP" title="Battery 3 Header" start_offset="03A9" end_offset="03B3" />
 <Panel header="SMIP" title="Battery 3 Temp Range 4" start_offset="03B4" end_offset="03C0" />
 <Panel header="SMIP" title="Battery 3 Temp Range 3" start_offset="03C1" end_offset="03CD" />
 <Panel header="SMIP" title="Battery 3 Temp Range 2" start_offset="03CE" end_offset="03DA" />
 <Panel header="SMIP" title="Battery 3 Temp Range 1" start_offset="03DB" end_offset="03E9" />
 <Panel header="SMIP" title="Battery 4 Header" start_offset="03EA" end_offset="03F4" />
 <Panel header="SMIP" title="Battery 4 Temp Range 4" start_offset="03F5" end_offset="0401" />
 <Panel header="SMIP" title="Battery 4 Temp Range 3" start_offset="0402" end_offset="040E" />
 <Panel header="SMIP" title="Battery 4 Temp Range 2" start_offset="040F" end_offset="041B" />
 <Panel header="SMIP" title="Battery 4 Temp Range 1" start_offset="041C" end_offset="042A" />
 <Panel header="SMIP" title="Battery 5 Header" start_offset="042B" end_offset="0435" />
 <Panel header="SMIP" title="Battery 5 Temp Range 4" start_offset="0436" end_offset="0442" />
 <Panel header="SMIP" title="Battery 5 Temp Range 3" start_offset="0443" end_offset="044F" />
 <Panel header="SMIP" title="Battery 5 Temp Range 2" start_offset="0450" end_offset="045C" />
 <Panel header="SMIP" title="Battery 5 Temp Range 1" start_offset="045D" end_offset="046B" />
 <Panel header="SMIP" title="BCU" start_offset="046C" end_offset="047F" />
 <Panel header="SMIP" title="Public Security Key 0" start_offset="0800" end_offset="08FF" class_type="UMGFWTK.OSSecurityKeyViewer" />
 <Panel header="SMIP" title="Public Security Key 1" start_offset="0900" end_offset="09FF" class_type="UMGFWTK.OSSecurityKeyViewer" />
 <Panel header="SMIP" title="Public Security Key 2" start_offset="0A00" end_offset="0AFF" class_type="UMGFWTK.OSSecurityKeyViewer" />
 <Panel header="SMIP" title="Public Security Key 3" start_offset="0B00" end_offset="0BFF" class_type="UMGFWTK.OSSecurityKeyViewer" />
 <Panel header="SMIP" title="Public Security Key 4" start_offset="0C00" end_offset="0CFF" class_type="UMGFWTK.OSSecurityKeyViewer" />
 <Panel header="SMIP" title="Reserved" start_offset="0D00" end_offset="FE00" />
 <Panel header="UMIP" title="Unsigned MIP" start_offset="0000" end_offset="0007" />
 <Panel header="UMIP" title="Calculated CheckSum Table" start_offset="0008" end_offset="01FF" />
 <Panel header="UMIP" title="Intel Reserved 0x200" start_offset="0200" end_offset="03FF" />
 <Panel header="UMIP" title="Software Revocation Table" start_offset="0400" end_offset="0407" />
 <Panel header="UMIP" title="Intel Reserved 0x408" start_offset="0408" end_offset="04FF" />
 <Panel header="UMIP" title="Minimum Versions" start_offset="0500" end_offset="050B" />
 <Panel header="UMIP" title="Intel Reserved 0x050C" start_offset="050C" end_offset="060B" />
 <Panel header="UMIP" title="PTI Hooks" start_offset="060C" end_offset="064B" />
 <Panel header="UMIP" title="Reserved 0x64C" start_offset="064C" end_offset="07FF" />
 <Panel header="UMIP" title="Energy Management Header" start_offset="0800" end_offset="0817" />
 <Panel header="UMIP" title="Theoretical Characteristics of Battery Fuel Gauge " start_offset="0818" end_offset="0894" />
 <Panel header="UMIP" title="Fuel Gauge Table Config" start_offset="0895" end_offset="0896" />
 <Panel header="UMIP" title="Fuel Gauge Table (Bat 1) Revision" start_offset="0897" end_offset="0898" />
 <Panel header="UMIP" title="Fuel Gauge Table (Bat 1) Data" start_offset="0899" end_offset="08A8" />
 <Panel header="UMIP" title="Fuel Gauge Table (Bat 2) Revision" start_offset="0927" end_offset="0928" />
 <Panel header="UMIP" title="Fuel Gauge Table (Bat 2) Data" start_offset="0929" end_offset="0938" />
 <Panel header="UMIP" title="Fuel Gauge Table (Bat 3) Revision" start_offset="09B7" end_offset="09B8" />
 <Panel header="UMIP" title="Fuel Gauge Table (Bat 3) Data" start_offset="09B9" end_offset="09C8" />
 <Panel header="UMIP" title="Fuel Gauge Table (Bat 4) Revision" start_offset="0A47" end_offset="0A48" />
 <Panel header="UMIP" title="Fuel Gauge Table (Bat 4) Data" start_offset="0A49" end_offset="0A58" />
 <Panel header="UMIP" title="Fuel Gauge Table (Bat 5) Revision" start_offset="0AD7" end_offset="0AD8" />
 <Panel header="UMIP" title="Fuel Gauge Table (Bat 5) Data" start_offset="0AD9" end_offset="0AE8" />
 <Panel header="UMIP" title="Intel Reserved 0xC00" start_offset="0C00" end_offset="3FFF" />
 <Panel header="UMIP" title="Intel Security Firmware" start_offset="6000" end_offset="7FFF" />
 <Panel header="UMIP" title="OEM Security Firmware" start_offset="8000" end_offset="FFFF" />
 <Panel header="FIP" title="Firmware Image Profile Header" start_offset="0000" end_offset="008F" />
 <!-- SMIP -->
 <header title="Signed Master Image Profile (SMIP) Header">
   <hdr_node name="Signature" byte_cnt="04" offset="0000" readonly="true">
     <data>SMIP</data>
   </hdr_node>
   <hdr_node name="Header Size" byte_cnt="02" offset="0004" readonly="true">
     <data>3F80</data>
   </hdr_node>
   <hdr_node name="Header Revision" byte_cnt="01" offset="0006">
     <data>03</data>
   </hdr_node>
   <hdr_node name="Header Checksum" byte_cnt="01" offset="0007" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="MBIS" byte_cnt="04" offset="0008">
     <data>00000000</data>
   </hdr_node>
   <!-- END SMIP -->
   <!-- Begin Misc SMIP Fields -->
   <hdr_node name="System Memory Geometry Profile" byte_cnt="04" offset="000C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="PUnit RComp Frequency Control" byte_cnt="04" offset="0010">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="OS Image Index OSDX" byte_cnt="01" offset="0014">
     <data>00</data>
   </hdr_node>
   <hdr_node name="BFSR" byte_cnt="01" offset="0015">
     <data>FF</data>
   </hdr_node>
   <hdr_node name="PUnit Response Timeout" byte_cnt="01" offset="0016">
     <data>00</data>
   </hdr_node>
   <hdr_node name="VID Control" byte_cnt="01" offset="0017">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reset Options" byte_cnt="01" offset="0018">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Vcc_C6 VID Override" byte_cnt="01" offset="0019">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Vnn VID Override" byte_cnt="01" offset="001A">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Vcc VID Override" byte_cnt="01" offset="001B">
     <data>00</data>
   </hdr_node>
   <!-- END Misc SMIP Fields -->
   <!-- BEGIN ISPV -->
   <hdr_node name="Initial Subsystem Power State" byte_cnt="16" offset="001C">
     <data>00000000000000000000000000000000</data>
   </hdr_node>
   <!-- END ISPV -->
   <!-- BEGIN MSIC Regs -->
   <hdr_node name="SCU RCOMP FC" byte_cnt="04" offset="002C">
     <data>BFBF3F3F</data>
   </hdr_node>
   <!-- END MSIC Regs -->
   <!-- BEGIN MSIC Charger Regs-->
   <hdr_node name="CHRGVOLT (Safe Setting)" byte_cnt="1" offset="0030">
     <data>23</data>
   </hdr_node>
   <hdr_node name="CHRGCRNT (Safe Setting)" byte_cnt="1" offset="0031">
     <data>00</data>
   </hdr_node>
   <hdr_node name="CHRGTIMER (Safe Setting)" byte_cnt="1" offset="0032">
     <data>01</data>
   </hdr_node>
   <hdr_node name="MSIC Charger Registers 1.3-RSVD" byte_cnt="1" offset="0033">
     <data>00</data>
   </hdr_node>
   <hdr_node name="MSIC Charger Registers 2" byte_cnt="4" offset="0034">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="MSIC Charger Registers 3" byte_cnt="4" offset="0038">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="MSIC Charger Registers 4" byte_cnt="4" offset="003C">
     <data>00000000</data>
   </hdr_node>
   <!-- END MSIC Charger Regs-->
   <!-- BEGIN MSIC VRO-->
   <hdr_node name="MSIC VRO" byte_cnt="04" offset="0040">
     <data>00000000</data>
   </hdr_node>
   <!-- END MSIC VRO-->
   <!-- BEGIN VFLAGS-->
   <hdr_node name="VFLAGS" byte_cnt="04" offset="0044">
     <data>00000000</data>
   </hdr_node>
   <!-- END VFLAGS-->
   <!-- BEGIN FHOB-->
   <hdr_node name="FW Handoff Buffer DWORD #0" byte_cnt="04" offset="0048">
     <data>00000004</data>
   </hdr_node>
   <hdr_node name="FW Handoff Buffer DWORD #1" byte_cnt="04" offset="004C">
     <data>00000001</data>
   </hdr_node>
   <hdr_node name="FW Handoff Buffer DWORD #2" byte_cnt="04" offset="0050">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="FW Handoff Buffer DWORD #3" byte_cnt="04" offset="0054">
     <data>00000000</data>
   </hdr_node>
   <!-- END FHOB-->
   <!-- BEGIN VRDT-->
   <hdr_node name="Voltage Rail Delay Table (VRDT)" byte_cnt="04" offset="0058">
     <data>05050505</data>
   </hdr_node>
   <hdr_node name="Voltage Rail Delay Table (VRDT)" byte_cnt="04" offset="005C">
     <data>05050505</data>
   </hdr_node>
   <hdr_node name="Voltage Rail Delay Table (VRDT)" byte_cnt="04" offset="0060">
     <data>05050505</data>
   </hdr_node>
   <hdr_node name="Voltage Rail Delay Table (VRDT)" byte_cnt="04" offset="0064">
     <data>05050505</data>
   </hdr_node>
   <hdr_node name="Voltage Rail Delay Table (VRDT)" byte_cnt="04" offset="0068">
     <data>05050505</data>
   </hdr_node>
   <hdr_node name="Voltage Rail Delay Table (VRDT)" byte_cnt="04" offset="006C">
     <data>05050505</data>
   </hdr_node>
   <hdr_node name="Voltage Rail Delay Table (VRDT)" byte_cnt="04" offset="0070">
     <data>05050505</data>
   </hdr_node>
   <hdr_node name="Voltage Rail Delay Table (VRDT)" byte_cnt="04" offset="0074">
     <data>05050505</data>
   </hdr_node>
   <!-- END VRDT-->
   <!-- BEGIN DEBUG MASKS-->
   <hdr_node name="Isolation Enable XOR Mask (IEXM)" byte_cnt="04" offset="0078">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Isolation Enable XOR Mask (IEXM)" byte_cnt="04" offset="007C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Power Gate Enable XOR Mask (PGEXM)" byte_cnt="04" offset="0080">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Power Gate Enable XOR Mask (PGEXM)" byte_cnt="04" offset="0084">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Clock Enable XOR Mask (CEXM)" byte_cnt="04" offset="0088">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Clock Enable XOR Mask (CEXM)" byte_cnt="04" offset="008C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Subsystem Reset XOR Mask (SSRXM)" byte_cnt="04" offset="0090">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Subsystem Reset XOR Mask (SSRXM)" byte_cnt="04" offset="0094">
     <data>00000000</data>
   </hdr_node>
   <!-- END DEBUG MASKS-->
   <!-- BEGIN IMR Regs-->
   <hdr_node name="IMR 0 BTYE 0" byte_cnt="4" offset="0098">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 0 BTYE 1" byte_cnt="4" offset="009C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 0 BTYE 2" byte_cnt="4" offset="00A0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 0 BTYE 3" byte_cnt="4" offset="00A4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 1 BTYE 0" byte_cnt="4" offset="00A8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 1 BTYE 1" byte_cnt="4" offset="00AC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 1 BTYE 2" byte_cnt="4" offset="00B0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 1 BTYE 3" byte_cnt="4" offset="00B4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 2 BTYE 0" byte_cnt="4" offset="00B8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 2 BTYE 1" byte_cnt="4" offset="00BC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 2 BTYE 2" byte_cnt="4" offset="00C0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 2 BTYE 3" byte_cnt="4" offset="00C4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 3 BTYE 0" byte_cnt="4" offset="00C8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 3 BTYE 1" byte_cnt="4" offset="00CC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 3 BTYE 2" byte_cnt="4" offset="00D0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 3 BTYE 3" byte_cnt="4" offset="00D4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 4 BTYE 0" byte_cnt="4" offset="00D8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 4 BTYE 1" byte_cnt="4" offset="00DC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 4 BTYE 2" byte_cnt="4" offset="00E0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 4 BTYE 3" byte_cnt="4" offset="00E4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 5 BTYE 0" byte_cnt="4" offset="00E8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 5 BTYE 1" byte_cnt="4" offset="00EC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 5 BTYE 2" byte_cnt="4" offset="00F0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 5 BTYE 3" byte_cnt="4" offset="00F4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 6 BTYE 0" byte_cnt="4" offset="00F8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 6 BTYE 1" byte_cnt="4" offset="00FC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 6 BTYE 2" byte_cnt="4" offset="0100">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 6 BTYE 3" byte_cnt="4" offset="0104">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 7 BTYE 0" byte_cnt="4" offset="0108">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 7 BTYE 1" byte_cnt="4" offset="010C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 7 BTYE 2" byte_cnt="4" offset="0110">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IMR 7 BTYE 3" byte_cnt="4" offset="0114">
     <data>00000000</data>
   </hdr_node>
   <!-- END IMR Regs-->
   <!-- BEGIN GPIO GCR0 -->
   <hdr_node name="GPDR0" byte_cnt="4" offset="0118">
     <data>00BF0000</data>
   </hdr_node>
   <hdr_node name="GPDR1" byte_cnt="4" offset="011C">
     <data>0B841304</data>
   </hdr_node>
   <hdr_node name="GPDR2" byte_cnt="4" offset="0120">
     <data>000B8E06</data>
   </hdr_node>
   <hdr_node name="GPSR0" byte_cnt="4" offset="0124">
     <data>00010000</data>
   </hdr_node>
   <hdr_node name="GPSR1" byte_cnt="4" offset="0128">
     <data>01840400</data>
   </hdr_node>
   <hdr_node name="GPSR2" byte_cnt="4" offset="012C">
     <data>00000006</data>
   </hdr_node>
   <hdr_node name="GRER0" byte_cnt="4" offset="0130">
     <data>40003003</data>
   </hdr_node>
   <hdr_node name="GRER1" byte_cnt="4" offset="0134">
     <data>00020000</data>
   </hdr_node>
   <hdr_node name="GRER2" byte_cnt="4" offset="0138">
     <data>00007000</data>
   </hdr_node>
   <hdr_node name="GFER0" byte_cnt="4" offset="013C">
     <data>C000100C</data>
   </hdr_node>
   <hdr_node name="GFER1" byte_cnt="4" offset="0140">
     <data>8009C8F0</data>
   </hdr_node>
   <hdr_node name="GFER2" byte_cnt="4" offset="0144">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="GAFR0_L" byte_cnt="4" offset="0148">
     <data>10555500</data>
   </hdr_node>
   <hdr_node name="GAFR0_U" byte_cnt="4" offset="014C">
     <data>05555695</data>
   </hdr_node>
   <hdr_node name="GAFR1_L" byte_cnt="4" offset="0150">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="GAFR1_U" byte_cnt="4" offset="0154">
     <data>00554010</data>
   </hdr_node>
   <hdr_node name="GAFR2_L" byte_cnt="4" offset="0158">
     <data>00055565</data>
   </hdr_node>
   <hdr_node name="GAFR2_U" byte_cnt="4" offset="015C">
     <data>00000055</data>
   </hdr_node>
   <!-- END GPIO GCR0 -->
   <!-- BEGIN GPIO GCR1 -->
   <hdr_node name="GPDR3" byte_cnt="4" offset="0160">
     <data>A8777000</data>
   </hdr_node>
   <hdr_node name="GPDR4" byte_cnt="4" offset="0164">
     <data>FFE80003</data>
   </hdr_node>
   <hdr_node name="GPDR5" byte_cnt="4" offset="0168">
     <data>00077E06</data>
   </hdr_node>
   <hdr_node name="GPSR3" byte_cnt="4" offset="016C">
     <data>A8600000</data>
   </hdr_node>
   <hdr_node name="GPSR4" byte_cnt="4" offset="0170">
     <data>2FE00000</data>
   </hdr_node>
   <hdr_node name="GPSR5" byte_cnt="4" offset="0174">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="GRER3" byte_cnt="4" offset="0178">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="GRER4" byte_cnt="4" offset="017C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="GRER5" byte_cnt="4" offset="0180">
     <data>00008000</data>
   </hdr_node>
   <hdr_node name="GFER3" byte_cnt="4" offset="0184">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="GFER4" byte_cnt="4" offset="0188">
     <data>00020000</data>
   </hdr_node>
   <hdr_node name="GFER5" byte_cnt="4" offset="018C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="GAFR3_L" byte_cnt="4" offset="0190">
     <data>00005000</data>
   </hdr_node>
   <hdr_node name="GAFR3_U" byte_cnt="4" offset="0194">
     <data>55500000</data>
   </hdr_node>
   <hdr_node name="GAFR4_L" byte_cnt="4" offset="0198">
     <data>55555540</data>
   </hdr_node>
   <hdr_node name="GAFR4_U" byte_cnt="4" offset="019C">
     <data>55555551</data>
   </hdr_node>
   <hdr_node name="GAFR5_L" byte_cnt="4" offset="01A0">
     <data>00000011</data>
   </hdr_node>
   <hdr_node name="GAFR5_U" byte_cnt="4" offset="01A4">
     <data>00000000</data>
   </hdr_node>
   <!-- END GPIO GCR1-->
   <!-- BEGIN MSIC VR REGS -->
   <hdr_node name="MSIC_VR_REGS0" byte_cnt="4" offset="01A8">
     <data>BF363636</data>
   </hdr_node>
   <hdr_node name="MSIC_VR_REGS1" byte_cnt="4" offset="01AC">
     <data>76E4B6B6</data>
   </hdr_node>
   <hdr_node name="MSIC_VR_REGS2" byte_cnt="4" offset="01B0">
     <data>36367676</data>
   </hdr_node>
   <hdr_node name="MSIC_VR_REGS3" byte_cnt="4" offset="01B4">
     <data>36363606</data>
   </hdr_node>
   <hdr_node name="MSIC_VR_REGS4" byte_cnt="4" offset="01B8">
     <data>06C7C736</data>
   </hdr_node>
   <hdr_node name="MSIC_VR_REGS5" byte_cnt="4" offset="01BC">
     <data>F6360606</data>
   </hdr_node>
   <hdr_node name="MSIC_VR_REGS6" byte_cnt="4" offset="01C0">
     <data>0080A406</data>
   </hdr_node>
   <hdr_node name="MSIC_VR_REGS7" byte_cnt="4" offset="01C4">
     <data>00000000</data>
   </hdr_node>
   <!-- END MSIC VR REGS -->
   <!-- BEGIN RD REGS -->
   <hdr_node name="RD1" byte_cnt="04" offset="01C8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="RD2" byte_cnt="04" offset="01CC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="RD3" byte_cnt="04" offset="01D0">
     <data>00000000</data>
   </hdr_node>
   <!-- END RD REGS -->
   <!-- BEGIN GP PIN REGS -->
   <hdr_node name="i2s_2_clk" byte_cnt="01" offset="01D4">
     <data>05</data>
   </hdr_node>
   <hdr_node name="i2s_2_fs" byte_cnt="01" offset="01D5">
     <data>05</data>
   </hdr_node>
   <hdr_node name="i2s_2_rxd" byte_cnt="01" offset="01D6">
     <data>05</data>
   </hdr_node>
   <hdr_node name="i2s_2_txd" byte_cnt="01" offset="01D7">
     <data>00</data>
   </hdr_node>
   <hdr_node name="msic_reset_b" byte_cnt="01" offset="01D8">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_0_clk" byte_cnt="01" offset="01D9">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_0_sdi" byte_cnt="01" offset="01DA">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_0_sdo" byte_cnt="01" offset="01DB">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_0_ss" byte_cnt="01" offset="01DC">
     <data>00</data>
   </hdr_node>
   <hdr_node name="svid_clkout" byte_cnt="01" offset="01DD">
     <data>00</data>
   </hdr_node>
   <hdr_node name="svid_clksynch" byte_cnt="01" offset="01DE">
     <data>00</data>
   </hdr_node>
   <hdr_node name="svid_din" byte_cnt="01" offset="01DF">
     <data>00</data>
   </hdr_node>
   <hdr_node name="svid_dout" byte_cnt="01" offset="01E0">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_clk" byte_cnt="01" offset="01E1">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_data[0]" byte_cnt="01" offset="01E2">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_data[1]" byte_cnt="01" offset="01E3">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_data[2]" byte_cnt="01" offset="01E4">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_data[3]" byte_cnt="01" offset="01E5">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_data[4]" byte_cnt="01" offset="01E6">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_data[5]" byte_cnt="01" offset="01E7">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_data[6]" byte_cnt="01" offset="01E8">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_data[7]" byte_cnt="01" offset="01E9">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_dir" byte_cnt="01" offset="01EA">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_nxt" byte_cnt="01" offset="01EB">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_refclk" byte_cnt="01" offset="01EC">
     <data>00</data>
   </hdr_node>
   <hdr_node name="usb_ulpi_stp" byte_cnt="01" offset="01ED">
     <data>00</data>
   </hdr_node>
   <hdr_node name="kbd_dkin[0]" byte_cnt="01" offset="01EE">
     <data>00</data>
   </hdr_node>
   <hdr_node name="kbd_dkin[1]" byte_cnt="01" offset="01EF">
     <data>05</data>
   </hdr_node>
   <hdr_node name="kbd_dkin[2]" byte_cnt="01" offset="01F0">
     <data>05</data>
   </hdr_node>
   <hdr_node name="kbd_dkin[3]" byte_cnt="01" offset="01F1">
     <data>01</data>
   </hdr_node>
   <hdr_node name="kbd_mkin[0]" byte_cnt="01" offset="01F2">
     <data>20</data>
   </hdr_node>
   <hdr_node name="kbd_mkin[1]" byte_cnt="01" offset="01F3">
     <data>00</data>
   </hdr_node>
   <hdr_node name="kbd_mkin[2]" byte_cnt="01" offset="01F4">
     <data>0D</data>
   </hdr_node>
   <hdr_node name="kbd_mkin[3]" byte_cnt="01" offset="01F5">
     <data>0D</data>
   </hdr_node>
   <hdr_node name="kbd_mkin[4]" byte_cnt="01" offset="01F6">
     <data>05</data>
   </hdr_node>
   <hdr_node name="kbd_mkin[5]" byte_cnt="01" offset="01F7">
     <data>05</data>
   </hdr_node>
   <hdr_node name="kbd_mkin[6]" byte_cnt="01" offset="01F8">
     <data>00</data>
   </hdr_node>
   <hdr_node name="kbd_mkin[7]" byte_cnt="01" offset="01F9">
     <data>00</data>
   </hdr_node>
   <hdr_node name="kbd_mkout[0]" byte_cnt="01" offset="01FA">
     <data>C8</data>
   </hdr_node>
   <hdr_node name="kbd_mkout[1]" byte_cnt="01" offset="01FB">
     <data>05</data>
   </hdr_node>
   <hdr_node name="kbd_mkout[2]" byte_cnt="01" offset="01FC">
     <data>20</data>
   </hdr_node>
   <hdr_node name="kbd_mkout[3]" byte_cnt="01" offset="01FD">
     <data>05</data>
   </hdr_node>
   <hdr_node name="kbd_mkout[4]" byte_cnt="01" offset="01FE">
     <data>05</data>
   </hdr_node>
   <hdr_node name="kbd_mkout[5]" byte_cnt="01" offset="01FF">
     <data>05</data>
   </hdr_node>
   <hdr_node name="kbd_mkout[6]" byte_cnt="01" offset="0200">
     <data>05</data>
   </hdr_node>
   <hdr_node name="kbd_mkout[7]" byte_cnt="01" offset="0201">
     <data>C8</data>
   </hdr_node>
   <hdr_node name="camerasb[10]" byte_cnt="01" offset="0202">
     <data>20</data>
   </hdr_node>
   <hdr_node name="camerasb[4]" byte_cnt="01" offset="0203">
     <data>20</data>
   </hdr_node>
   <hdr_node name="camerasb[5]" byte_cnt="01" offset="0204">
     <data>20</data>
   </hdr_node>
   <hdr_node name="camerasb[6]" byte_cnt="01" offset="0205">
     <data>20</data>
   </hdr_node>
   <hdr_node name="camerasb[7]" byte_cnt="01" offset="0206">
     <data>01</data>
   </hdr_node>
   <hdr_node name="camerasb[8]" byte_cnt="01" offset="0207">
     <data>20</data>
   </hdr_node>
   <hdr_node name="camerasb[9]" byte_cnt="01" offset="0208">
     <data>20</data>
   </hdr_node>
   <hdr_node name="i2c_4_scl" byte_cnt="01" offset="0209">
     <data>66</data>
   </hdr_node>
   <hdr_node name="i2c_4_sda" byte_cnt="01" offset="020A">
     <data>66</data>
   </hdr_node>
   <hdr_node name="i2c_5_scl" byte_cnt="01" offset="020B">
     <data>66</data>
   </hdr_node>
   <hdr_node name="i2c_5_sda" byte_cnt="01" offset="020C">
     <data>66</data>
   </hdr_node>
   <hdr_node name="intd_dsi_te1" byte_cnt="01" offset="020D">
     <data>01</data>
   </hdr_node>
   <hdr_node name="intd_dsi_te2" byte_cnt="01" offset="020E">
     <data>01</data>
   </hdr_node>
   <hdr_node name="stio_0_cd_b" byte_cnt="01" offset="020F">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_0_clk" byte_cnt="01" offset="0210">
     <data>00</data>
   </hdr_node>
   <hdr_node name="stio_0_cmd" byte_cnt="01" offset="0211">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_0_dat[0]" byte_cnt="01" offset="0212">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_0_dat[1]" byte_cnt="01" offset="0213">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_0_dat[2]" byte_cnt="01" offset="0214">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_0_dat[3]" byte_cnt="01" offset="0215">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_0_dat[4]" byte_cnt="01" offset="0216">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_0_dat[5]" byte_cnt="01" offset="0217">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_0_dat[6]" byte_cnt="01" offset="0218">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_0_dat[7]" byte_cnt="01" offset="0219">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_0_wp_b" byte_cnt="01" offset="021A">
     <data>02</data>
   </hdr_node>
   <hdr_node name="camerasb[0]" byte_cnt="01" offset="021B">
     <data>21</data>
   </hdr_node>
   <hdr_node name="camerasb[1]" byte_cnt="01" offset="021C">
     <data>45</data>
   </hdr_node>
   <hdr_node name="camerasb[2]" byte_cnt="01" offset="021D">
     <data>21</data>
   </hdr_node>
   <hdr_node name="camerasb[3]" byte_cnt="01" offset="021E">
     <data>21</data>
   </hdr_node>
   <hdr_node name="ded_gpio[10]" byte_cnt="01" offset="021F">
     <data>01</data>
   </hdr_node>
   <hdr_node name="ded_gpio[11]" byte_cnt="01" offset="0220">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[12]" byte_cnt="01" offset="0221">
     <data>21</data>
   </hdr_node>
   <hdr_node name="ded_gpio[13]" byte_cnt="01" offset="0222">
     <data>21</data>
   </hdr_node>
   <hdr_node name="ded_gpio[14]" byte_cnt="01" offset="0223">
     <data>21</data>
   </hdr_node>
   <hdr_node name="ded_gpio[15]" byte_cnt="01" offset="0224">
     <data>01</data>
   </hdr_node>
   <hdr_node name="ded_gpio[16]" byte_cnt="01" offset="0225">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[17]" byte_cnt="01" offset="0226">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[18]" byte_cnt="01" offset="0227">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[19]" byte_cnt="01" offset="0228">
     <data>01</data>
   </hdr_node>
   <hdr_node name="ded_gpio[20]" byte_cnt="01" offset="0229">
     <data>21</data>
   </hdr_node>
   <hdr_node name="ded_gpio[21]" byte_cnt="01" offset="022A">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[22]" byte_cnt="01" offset="022B">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[23]" byte_cnt="01" offset="022C">
     <data>05</data>
   </hdr_node>
   <hdr_node name="ded_gpio[24]" byte_cnt="01" offset="022D">
     <data>35</data>
   </hdr_node>
   <hdr_node name="ded_gpio[25]" byte_cnt="01" offset="022E">
     <data>05</data>
   </hdr_node>
   <hdr_node name="ded_gpio[26]" byte_cnt="01" offset="022F">
     <data>35</data>
   </hdr_node>
   <hdr_node name="ded_gpio[27]" byte_cnt="01" offset="0230">
     <data>05</data>
   </hdr_node>
   <hdr_node name="ded_gpio[28]" byte_cnt="01" offset="0231">
     <data>35</data>
   </hdr_node>
   <hdr_node name="ded_gpio[29]" byte_cnt="01" offset="0232">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[30]" byte_cnt="01" offset="0233">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[8]" byte_cnt="01" offset="0234">
     <data>01</data>
   </hdr_node>
   <hdr_node name="ded_gpio[9]" byte_cnt="01" offset="0235">
     <data>01</data>
   </hdr_node>
   <hdr_node name="mpti_nidnt_clk" byte_cnt="01" offset="0236">
     <data>00</data>
   </hdr_node>
   <hdr_node name="mpti_nidnt_data[0]" byte_cnt="01" offset="0237">
     <data>00</data>
   </hdr_node>
   <hdr_node name="mpti_nidnt_data[1]" byte_cnt="01" offset="0238">
     <data>00</data>
   </hdr_node>
   <hdr_node name="mpti_nidnt_data[2]" byte_cnt="01" offset="0239">
     <data>00</data>
   </hdr_node>
   <hdr_node name="mpti_nidnt_data[3]" byte_cnt="01" offset="023A">
     <data>00</data>
   </hdr_node>
   <hdr_node name="stio_1_clk" byte_cnt="01" offset="023B">
     <data>20</data>
   </hdr_node>
   <hdr_node name="stio_1_cmd" byte_cnt="01" offset="023C">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_1_dat[0]" byte_cnt="01" offset="023D">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_1_dat[1]" byte_cnt="01" offset="023E">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_1_dat[2]" byte_cnt="01" offset="023F">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_1_dat[3]" byte_cnt="01" offset="0240">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_2_clk" byte_cnt="01" offset="0241">
     <data>20</data>
   </hdr_node>
   <hdr_node name="stio_2_cmd" byte_cnt="01" offset="0242">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_2_dat[0]" byte_cnt="01" offset="0243">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_2_dat[1]" byte_cnt="01" offset="0244">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_2_dat[2]" byte_cnt="01" offset="0245">
     <data>05</data>
   </hdr_node>
   <hdr_node name="stio_2_dat[3]" byte_cnt="01" offset="0246">
     <data>05</data>
   </hdr_node>
   <hdr_node name="coms_int[0]" byte_cnt="01" offset="0247">
     <data>01</data>
   </hdr_node>
   <hdr_node name="coms_int[1]" byte_cnt="01" offset="0248">
     <data>05</data>
   </hdr_node>
   <hdr_node name="coms_int[2]" byte_cnt="01" offset="0249">
     <data>05</data>
   </hdr_node>
   <hdr_node name="coms_int[3]" byte_cnt="01" offset="024A">
     <data>05</data>
   </hdr_node>
   <hdr_node name="ded_gpio[4]" byte_cnt="01" offset="024B">
     <data>00</data>
   </hdr_node>
   <hdr_node name="ded_gpio[5]" byte_cnt="01" offset="024C">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[6]" byte_cnt="01" offset="024D">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[7]" byte_cnt="01" offset="024E">
     <data>20</data>
   </hdr_node>
   <hdr_node name="i2s_0_clk" byte_cnt="01" offset="024F">
     <data>20</data>
   </hdr_node>
   <hdr_node name="i2s_0_fs" byte_cnt="01" offset="0250">
     <data>20</data>
   </hdr_node>
   <hdr_node name="i2s_0_rxd" byte_cnt="01" offset="0251">
     <data>00</data>
   </hdr_node>
   <hdr_node name="i2s_0_txd" byte_cnt="01" offset="0252">
     <data>20</data>
   </hdr_node>
   <hdr_node name="i2s_1_clk" byte_cnt="01" offset="0253">
     <data>20</data>
   </hdr_node>
   <hdr_node name="i2s_1_fs" byte_cnt="01" offset="0254">
     <data>20</data>
   </hdr_node>
   <hdr_node name="i2s_1_rxd" byte_cnt="01" offset="0255">
     <data>00</data>
   </hdr_node>
   <hdr_node name="i2s_1_txd" byte_cnt="01" offset="0256">
     <data>20</data>
   </hdr_node>
   <hdr_node name="mslim_1_bclk" byte_cnt="01" offset="0257">
     <data>01</data>
   </hdr_node>
   <hdr_node name="mslim_1_bdat" byte_cnt="01" offset="0258">
     <data>05</data>
   </hdr_node>
   <hdr_node name="resetout_b" byte_cnt="01" offset="0259">
     <data>75</data>
   </hdr_node>
   <hdr_node name="spi_2_clk" byte_cnt="01" offset="025A">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_2_sdi" byte_cnt="01" offset="025B">
     <data>01</data>
   </hdr_node>
   <hdr_node name="spi_2_sdo" byte_cnt="01" offset="025C">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_2_ss[0]" byte_cnt="01" offset="025D">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_2_ss[1]" byte_cnt="01" offset="025E">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_3_clk" byte_cnt="01" offset="025F">
     <data>C8</data>
   </hdr_node>
   <hdr_node name="spi_3_sdi" byte_cnt="01" offset="0260">
     <data>C8</data>
   </hdr_node>
   <hdr_node name="spi_3_sdo" byte_cnt="01" offset="0261">
     <data>C8</data>
   </hdr_node>
   <hdr_node name="spi_3_ss[0]" byte_cnt="01" offset="0262">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_3_ss[1]" byte_cnt="01" offset="0263">
     <data>0D</data>
   </hdr_node>
   <hdr_node name="uart_0_cts" byte_cnt="01" offset="0264">
     <data>05</data>
   </hdr_node>
   <hdr_node name="uart_0_rts" byte_cnt="01" offset="0265">
     <data>05</data>
   </hdr_node>
   <hdr_node name="uart_0_rx" byte_cnt="01" offset="0266">
     <data>05</data>
   </hdr_node>
   <hdr_node name="uart_0_tx" byte_cnt="01" offset="0267">
     <data>05</data>
   </hdr_node>
   <hdr_node name="uart_1_rx" byte_cnt="01" offset="0268">
     <data>05</data>
   </hdr_node>
   <hdr_node name="uart_1_sd" byte_cnt="01" offset="0269">
     <data>35</data>
   </hdr_node>
   <hdr_node name="uart_1_tx" byte_cnt="01" offset="026A">
     <data>35</data>
   </hdr_node>
   <hdr_node name="uart_2_rx" byte_cnt="01" offset="026B">
     <data>05</data>
   </hdr_node>
   <hdr_node name="uart_2_tx" byte_cnt="01" offset="026C">
     <data>05</data>
   </hdr_node>
   <hdr_node name="aclkph" byte_cnt="01" offset="026D">
     <data>01</data>
   </hdr_node>
   <hdr_node name="dclkph" byte_cnt="01" offset="026E">
     <data>01</data>
   </hdr_node>
   <hdr_node name="dsiclkph" byte_cnt="01" offset="026F">
     <data>01</data>
   </hdr_node>
   <hdr_node name="ierr" byte_cnt="01" offset="0270">
     <data>30</data>
   </hdr_node>
   <hdr_node name="jtag_tckc" byte_cnt="01" offset="0271">
     <data>00</data>
   </hdr_node>
   <hdr_node name="jtag_tdic" byte_cnt="01" offset="0272">
     <data>06</data>
   </hdr_node>
   <hdr_node name="jtag_tdoc" byte_cnt="01" offset="0273">
     <data>26</data>
   </hdr_node>
   <hdr_node name="jtag_tmsc" byte_cnt="01" offset="0274">
     <data>00</data>
   </hdr_node>
   <hdr_node name="jtag_trst_b" byte_cnt="01" offset="0275">
     <data>05</data>
   </hdr_node>
   <hdr_node name="lclkph" byte_cnt="01" offset="0276">
     <data>01</data>
   </hdr_node>
   <hdr_node name="lfhclkph" byte_cnt="01" offset="0277">
     <data>01</data>
   </hdr_node>
   <hdr_node name="osc_clk_ctrl[0]" byte_cnt="01" offset="0278">
     <data>05</data>
   </hdr_node>
   <hdr_node name="osc_clk_ctrl[1]" byte_cnt="01" offset="0279">
     <data>05</data>
   </hdr_node>
   <hdr_node name="osc_clk_out[0]" byte_cnt="01" offset="027A">
     <data>20</data>
   </hdr_node>
   <hdr_node name="osc_clk_out[1]" byte_cnt="01" offset="027B">
     <data>20</data>
   </hdr_node>
   <hdr_node name="osc_clk_out[2]" byte_cnt="01" offset="027C">
     <data>20</data>
   </hdr_node>
   <hdr_node name="osc_clk_out[3]" byte_cnt="01" offset="027D">
     <data>20</data>
   </hdr_node>
   <hdr_node name="prochot_b" byte_cnt="01" offset="027E">
     <data>45</data>
   </hdr_node>
   <hdr_node name="thermtrip_b" byte_cnt="01" offset="027F">
     <data>45</data>
   </hdr_node>
   <hdr_node name="uclkph" byte_cnt="01" offset="0280">
     <data>01</data>
   </hdr_node>
   <hdr_node name="ded_gpio[31]" byte_cnt="01" offset="0281">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[32]" byte_cnt="01" offset="0282">
     <data>20</data>
   </hdr_node>
   <hdr_node name="ded_gpio[33]" byte_cnt="01" offset="0283">
     <data>20</data>
   </hdr_node>
   <hdr_node name="hdmi_cec" byte_cnt="01" offset="0284">
     <data>45</data>
   </hdr_node>
   <hdr_node name="i2c_3_scl_hdmi_ddc" byte_cnt="01" offset="0285">
     <data>66</data>
   </hdr_node>
   <hdr_node name="i2c_3_sda_hdmi_ddc" byte_cnt="01" offset="0286">
     <data>66</data>
   </hdr_node>
   <hdr_node name="i2c_0_scl" byte_cnt="01" offset="0287">
     <data>66</data>
   </hdr_node>
   <hdr_node name="i2c_0_sda" byte_cnt="01" offset="0288">
     <data>66</data>
   </hdr_node>
   <hdr_node name="i2c_1_scl" byte_cnt="01" offset="0289">
     <data>66</data>
   </hdr_node>
   <hdr_node name="i2c_1_sda" byte_cnt="01" offset="028A">
     <data>66</data>
   </hdr_node>
   <hdr_node name="i2c_2_scl" byte_cnt="01" offset="028B">
     <data>66</data>
   </hdr_node>
   <hdr_node name="i2c_2_sda" byte_cnt="01" offset="028C">
     <data>66</data>
   </hdr_node>
   <hdr_node name="spi_1_clk" byte_cnt="01" offset="028D">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_1_sdi" byte_cnt="01" offset="028E">
     <data>01</data>
   </hdr_node>
   <hdr_node name="spi_1_sdo" byte_cnt="01" offset="028F">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_1_ss[0]" byte_cnt="01" offset="0290">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_1_ss[1]" byte_cnt="01" offset="0291">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_1_ss[2]" byte_cnt="01" offset="0292">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_1_ss[3]" byte_cnt="01" offset="0293">
     <data>00</data>
   </hdr_node>
   <hdr_node name="spi_1_ss[4]" byte_cnt="01" offset="0294">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved" byte_cnt="01" offset="0295">
     <data>00</data>
   </hdr_node>
   <!--End of Pinlist!-->
   <!-- END GP PIN REGS -->
   <!-- BEGIN Intel Reserved  -->
   <hdr_node name="Intel Reserved 0x0296" byte_cnt="06" offset="0296">
     <data>000000000000</data>
   </hdr_node>
   <!-- END Intel Reserved -->
   <!-- BEGIN SKIIA Security Key Index -->
   <hdr_node name="SKIIA Security Key Index (ia Key Index)" byte_cnt="01" offset="029C">
     <data>02</data>
   </hdr_node>
   <hdr_node name="SKIIA Security Key Index (os Key Index)" byte_cnt="01" offset="029D">
     <data>02</data>
   </hdr_node>
   <hdr_node name="SKIIA Security Key Index (ext. FW Key Index)" byte_cnt="01" offset="029E">
     <data>02</data>
   </hdr_node>
   <hdr_node name="SKIIA Security Key Index (RESERVED)" byte_cnt="01" offset="029F">
     <data>00</data>
   </hdr_node>
   <!-- END SKIIA Security Key Index -->
   <!-- BEGIN USB Descriptor -->
   <hdr_node name="USB Descriptor Overrides" byte_cnt="04" offset="02A0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Descriptor Overrides" byte_cnt="04" offset="02A4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Descriptor Overrides" byte_cnt="04" offset="02A8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Descriptor Overrides" byte_cnt="04" offset="02AC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Descriptor Overrides" byte_cnt="04" offset="02B0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Descriptor Overrides" byte_cnt="04" offset="02B4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Descriptor Overrides" byte_cnt="04" offset="02B8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Descriptor Overrides" byte_cnt="04" offset="02BC">
     <data>00000000</data>
   </hdr_node>
   <!-- END USB Descriptor -->
   <!-- BEGIN SLEW RATES -->
   <hdr_node name="Slew Rates Table" byte_cnt="04" offset="02C0">
     <data>0F0FF0FC</data>
   </hdr_node>
   <hdr_node name="Slew Rates Table" byte_cnt="04" offset="02C4">
     <data>88FAE173</data>
   </hdr_node>
   <hdr_node name="Slew Rates Table" byte_cnt="04" offset="02C8">
     <data>88FAE177</data>
   </hdr_node>
   <!-- END SLEW RATES -->
   <!-- BEGIN Reserved -->
   <hdr_node name="Wake Control Register" byte_cnt="04" offset="02CC">
     <data>00000000</data>
   </hdr_node>
   <!-- END Reserved -->
   <!-- BEGIN Security FW Sizes -->
   <hdr_node name="iCache Security FW Size" byte_cnt="4" offset="02D0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Resident Image Security FW Size" byte_cnt="4" offset="02D4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Extended Security FW Size" byte_cnt="4" offset="02D8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Reserved Security FW Size" byte_cnt="4" offset="02DC">
     <data>00000000</data>
   </hdr_node>
   <!-- END Security FW Sizes -->
   <!-- BEGIN RTC Reg -->
   <hdr_node name="RTC Config 2 Register" byte_cnt="4" offset="02E0">
     <data>00000073</data>
   </hdr_node>
   <!-- END RTC Regs -->
   <!-- BEGIN Battery Settings -->
   <hdr_node name="Battery Settings" byte_cnt="4" offset="02E4">
     <data>00001EB4</data>
   </hdr_node>
   <!-- END Battery Settings -->
   <!-- BEGIN VAUDA Config -->
   <hdr_node name="VAUDA Configuration for MSIC Rails" byte_cnt="4" offset="02E8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Product Name ..." byte_cnt="4" offset="02EC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Product Name" byte_cnt="4" offset="02F0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Product Manufacturer ..." byte_cnt="4" offset="02F4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Product Manufacturer" byte_cnt="4" offset="02F8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Product Serial" byte_cnt="4" offset="02FC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Product Serial" byte_cnt="4" offset="0300">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Product Serial" byte_cnt="4" offset="0304">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="USB Product Serial" byte_cnt="4" offset="0308">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="PTI Field" byte_cnt="4" offset="030C">
     <data>00000090</data>
   </hdr_node>
   <!-- END VAUDA Config -->
   <!-- BEGIN Reserved -->
   <hdr_node name="Intel Reserved 0x0310" byte_cnt="4" offset="0310">
     <data>00000000</data>
   </hdr_node>
   <!-- END  Reserved -->

   <!-- BEGIN SBCT - Supportted Battery Characteristics Table -->
   <hdr_node name="SBCT REV" byte_cnt="1" offset="0314">
     <data>16</data>
   </hdr_node>
   <hdr_node name="FPO (Fixed Platform Options)" byte_cnt="1" offset="0315">
     <data>4D</data>
   </hdr_node>
   <hdr_node name="FPO1 (Fixed Platform Options 1)" byte_cnt="1" offset="0316">
     <data>00</data>
   </hdr_node>
   <hdr_node name="RSYS mOhms (System Resistance)" byte_cnt="1" offset="0317">
     <data>AA</data>
   </hdr_node>
   <hdr_node name="VMIN mV" byte_cnt="2" offset="0318">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="VBATTCRIT mV" byte_cnt="2" offset="031A">
     <data>0DFC</data>
   </hdr_node>
   <hdr_node name="ITC (Termination Current)" byte_cnt="2" offset="031C">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="TSUL - Degrees C (Safe Temperature Upper Limit)" byte_cnt="2" offset="031E">
     <data>003C</data>
   </hdr_node>
   <hdr_node name="TSLL - Degrees C (Safe Temperature Lower Limit)" byte_cnt="2" offset="0320">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="BRDID" byte_cnt="1" offset="0322">
     <data>02</data>
   </hdr_node>
   <hdr_node name="Reserved 0323" byte_cnt="1" offset="0323">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved 0324" byte_cnt="2" offset="0324">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="BTFORMAT (NUM_BATTERIES/NUM_TEMP_RANGES)" byte_cnt="1" offset="0326">
     <data>13</data>
   </hdr_node>
   <!-- BEGIN  Battery #1 data-->
   <!-- BEGIN  Battery #1 Header data -->
   <hdr_node name="B1IDMIN (Battery #1 ID-MIN ADC Value)" byte_cnt="2" offset="0327">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B1IDMAX (Battery #1 ID-MAX ADC Value)" byte_cnt="2" offset="0329">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B1TYPE (Battery #1 Type)" byte_cnt="1" offset="032B">
     <data>02</data>
   </hdr_node>
   <hdr_node name="B1CAP (Battery #1 Capacity (mAH))" byte_cnt="2" offset="032C">
     <data>05D2</data>
   </hdr_node>
   <hdr_node name="B1VMAX lower (Battery #1 Max Voltage (mV))" byte_cnt="1" offset="032E">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B1VMAX upper (Battery #1 Max Voltage (mV))" byte_cnt="1" offset="032F">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B1LOWBATTLS (Battery #1 Low Setting-LOWBATTDET)" byte_cnt="1" offset="0330">
     <data>C7</data>
   </hdr_node>
   <hdr_node name="B1SAFE (Battery #1 Safe Voltage/Current Limit-CHRSAFELMT)" byte_cnt="1" offset="0331">
     <data>90</data>
   </hdr_node>
   <!-- END  Battery #1 Header data -->
   <!-- BEGIN  Battery #1 Temp Range 4 data -->
   <hdr_node name="B1T4UL (Temp Range 4 UL - Degrees C Upper Bound +60C)" byte_cnt="2" offset="0332">
     <data>003C</data>
   </hdr_node>
   <hdr_node name="B1T4PR (Temp Range 4 Pack Resistance (ohms))" byte_cnt="1" offset="0334">
     <data>AA</data>
   </hdr_node>
   <hdr_node name="B1T4FCV lower(Temp Range 4 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0335">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B1T4FCV upper(Temp Range 4 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0336">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B1T4FCI (Temp Range 4 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0337">
     <data>0145</data>
   </hdr_node>
   <hdr_node name="B1T4MCVSTART lower (Temp Range 4 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0339">
     <data>36</data>
   </hdr_node>
   <hdr_node name="B1T4MCVSTART upper (Temp Range 4 Maintenance Charge Voltage LT)" byte_cnt="1" offset="033A">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B1T4MCVSTOP (Temp Range 4 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="033B">
     <data>1068</data>
   </hdr_node>
   <hdr_node name="B1T4MCI lower(Temp Range 4 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="033D">
     <data>45</data>
   </hdr_node>
   <hdr_node name="B1T4MCI upper(Temp Range 4 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="033E">
     <data>01</data>
   </hdr_node>
   <!-- END    Battery #1 Temp Range 4 data -->
   <!-- BEGIN  Battery #1 Temp Range 3 data -->
   <hdr_node name="B1T3UL (Temp Range 3 UL - Degrees C Upper Bound +45C)" byte_cnt="2" offset="033F">
     <data>002D</data>
   </hdr_node>
   <hdr_node name="B1T3PR (Temp Range 3 Pack Resistance)" byte_cnt="1" offset="0341">
     <data>AA</data>
   </hdr_node>
   <hdr_node name="B1T3FCV lower(Temp Range 3 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0342">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B1T3FCV upper(Temp Range 3 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0343">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B1T3FCI (Temp Range 3 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0344">
     <data>0355</data>
   </hdr_node>
   <hdr_node name="B1T3MCVSTART lower(Temp Range 3 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0346">
     <data>36</data>
   </hdr_node>
   <hdr_node name="B1T3MCVSTART upper(Temp Range 3 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0347">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B1T3MCVSTOP (Temp Range 3 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="0348">
     <data>1068</data>
   </hdr_node>
   <hdr_node name="B1T3MCI lower(Temp Range 3 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="034A">
     <data>5B</data>
   </hdr_node>
   <hdr_node name="B1T3MCI upper(Temp Range 3 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="034B">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #1 Temp Range 3 data -->
   <!-- BEGIN  Battery #1 Temp Range 2 data -->
   <hdr_node name="B1T2UL (Temp Range 2 UL - Degrees C Upper Bound +10C)" byte_cnt="2" offset="034C">
     <data>000A</data>
   </hdr_node>
   <hdr_node name="B1T2PR (Temp Range 2 Pack Resistance)" byte_cnt="1" offset="034E">
     <data>AA</data>
   </hdr_node>
   <hdr_node name="B1T2FCV lower(Temp Range 2 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="034F">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B1T2FCV upper(Temp Range 2 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0350">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B1T2FCI (Temp Range 2 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0351">
     <data>0362</data>
   </hdr_node>
   <hdr_node name="B1T2MCVSTART lower(Temp Range 2 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0353">
     <data>36</data>
   </hdr_node>
   <hdr_node name="B1T2MCVSTART upper(Temp Range 2 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0354">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B1T2MCVSTOP (Temp Range 2 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="0355">
     <data>1068</data>
   </hdr_node>
   <hdr_node name="B1T2MCI lower(Temp Range 2 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0357">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B1T2MCI upper(Temp Range 2 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0358">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #1 Temp Range 2 data -->
   <!-- BEGIN  Battery #1 Temp Range 1 data -->
   <hdr_node name="B1T1UL (Temp Range 1 UL - Degrees C Upper Bound +0C)" byte_cnt="2" offset="0359">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B1T1PR (Temp Range 1 Pack Resistance)" byte_cnt="1" offset="035B">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B1T1FCV lower(Temp Range 1 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="035C">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B1T1FCV upper(Temp Range 1 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="035D">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B1T1FCI (Temp Range 1 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="035E">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B1T1MCVSTART lower(Temp Range 1 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0360">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B1T1MCVSTART upper(Temp Range 1 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0361">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B1T1MCVSTOP (Temp Range 1 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="0362">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B1T1MCI lower(Temp Range 1 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0364">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B1T1MCI upper(Temp Range 1 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0365">
     <data>00</data>
   </hdr_node>
   <!-- END    Battery #1 Temp Range 1 data -->
   <hdr_node name="B1T1LL (Temp Range 1 LL - Degrees C Lower Bound -10C)" byte_cnt="2" offset="0366">
     <data>800A</data>
   </hdr_node>
   <!-- END    Battery #1 Header data -->
   <!-- END  Battery #1 data-->
   <!-- BEGIN  Battery #2 data-->
   <!-- BEGIN  Battery #2 Header data -->
   <hdr_node name="B2IDMIN (Battery #2 ID-MIN ADC Value)" byte_cnt="2" offset="0368">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B2IDMAX (Battery #2 ID-MAX ADC Value)" byte_cnt="2" offset="036A">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B2TYPE (Battery #2 Type)" byte_cnt="1" offset="036C">
     <data>02</data>
   </hdr_node>
   <hdr_node name="B2CAP (Battery #2 Capacity (mAH))" byte_cnt="2" offset="036D">
     <data>05DC</data>
   </hdr_node>
   <hdr_node name="B2VMAX lower(Battery #2 Max Voltage (mV))" byte_cnt="1" offset="036F">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B2VMAX upper(Battery #2 Max Voltage (mV))" byte_cnt="1" offset="0370">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B2LOWBATTLS (Battery #2 Low Setting-LOWBATTDET)" byte_cnt="1" offset="0371">
     <data>02</data>
   </hdr_node>
   <hdr_node name="B2SAFE (Battery #2 Safe Voltage/Current Limit-CHRSAFELMT)" byte_cnt="1" offset="0372">
     <data>40</data>
   </hdr_node>
   <!-- END  Battery #2 Header data -->
   <!-- BEGIN  Battery #2 Temp Range 4 data -->
   <hdr_node name="B2T4UL (Temp Range 4 UL-Degrees C Upper Bound +60C)" byte_cnt="2" offset="0373">
     <data>003C</data>
   </hdr_node>
   <hdr_node name="B2T4PR (Temp Range 4 Pack Resistance)" byte_cnt="1" offset="0375">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T4FCV lower(Temp Range 4 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0376">
     <data>04</data>
   </hdr_node>
   <hdr_node name="B2T4FCV upper(Temp Range 4 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0377">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B2T4FCI (Temp Range 4 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0378">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B2T4MCVSTART lower(Temp Range 4 Maintenance Charge Voltage LT)" byte_cnt="1" offset="037A">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T4MCVSTART upper(Temp Range 4 Maintenance Charge Voltage LT)" byte_cnt="1" offset="037B">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T4MCVSTOP (Temp Range 4 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="037C">
     <data>0FC8</data>
   </hdr_node>
   <hdr_node name="B2T4MCI lower(Temp Range 4 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="037E">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B2T4MCI upper(Temp Range 4 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="037F">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #2 Temp Range 4 data -->
   <!-- BEGIN  Battery #2 Temp Range 3 data -->
   <hdr_node name="B2T3UL (Temp Range 3 UL - Degrees C Upper Bound +45C)" byte_cnt="2" offset="0380">
     <data>002D</data>
   </hdr_node>
   <hdr_node name="B2T3PR (Temp Range 3 Pack Resistance)" byte_cnt="1" offset="0382">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T3FCV lower(Temp Range 3 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0383">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B2T3FCV upper(Temp Range 3 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0384">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B2T3FCI (Temp Range 3 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0385">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B2T3MCVSTART lower(Temp Range 3 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0387">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T3MCVSTART upper(Temp Range 3 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0388">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T3MCVSTOP (Temp Range 3 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="0389">
     <data>102C</data>
   </hdr_node>
   <hdr_node name="B2T3MCI lower(Temp Range 3 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="038B">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B2T3MCI upper(Temp Range 3 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="038C">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #2 Temp Range 3 data -->
   <!-- BEGIN  Battery #2 Temp Range 2 data -->
   <hdr_node name="B2T2UL (Temp Range 2 UL - Degrees C Upper Bound +10C)" byte_cnt="2" offset="038D">
     <data>000A</data>
   </hdr_node>
   <hdr_node name="B2T2PR (Temp Range 2 Pack Resistance)" byte_cnt="1" offset="038F">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T2FCV lower(Temp Range 2 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0390">
     <data>04</data>
   </hdr_node>
   <hdr_node name="B2T2FCV upper(Temp Range 2 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0391">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B2T2FCI (Temp Range 2 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0392">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B2T2MCVSTART lower(Temp Range 2 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0394">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T2MCVSTART upper(Temp Range 2 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0395">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T2MCVSTOP (Temp Range 2 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="0396">
     <data>0FC8</data>
   </hdr_node>
   <hdr_node name="B2T2MCI lower(Temp Range 2 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0398">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B2T2MCI upper(Temp Range 2 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0399">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #2 Temp Range 2 data -->
   <!-- BEGIN  Battery #2 Temp Range 1 data -->
   <hdr_node name="B2T1UL (Temp Range 1 UL - Degrees C Upper Bound +0C)" byte_cnt="2" offset="039A">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B2T1PR (Temp Range 1 Pack Resistance)" byte_cnt="1" offset="039C">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T1FCV lower(Temp Range 1 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="039D">
     <data>64</data>
   </hdr_node>
   <hdr_node name="B2T1FCV upper(Temp Range 1 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="039E">
     <data>0F</data>
   </hdr_node>
   <hdr_node name="B2T1FCI (Temp Range 1 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="039F">
     <data>0190</data>
   </hdr_node>
   <hdr_node name="B2T1MCVSTART lower(Temp Range 1 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03A1">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T1MCVSTART upper(Temp Range 1 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03A2">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B2T1MCVSTOP (Temp Range 1 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="03A3">
     <data>0F64</data>
   </hdr_node>
   <hdr_node name="B2T1MCI lower(Temp Range 1 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="03A5">
     <data>90</data>
   </hdr_node>
   <hdr_node name="B2T1MCI upper(Temp Range 1 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="03A6">
     <data>01</data>
   </hdr_node>
   <!-- END    Battery #2 Temp Range 1 data -->
   <hdr_node name="B2T1LL (Temp Range 1 LL - Degrees C Lower Bound -10C)" byte_cnt="2" offset="03A7">
     <data>800A</data>
   </hdr_node>
   <!-- END    Battery #2 Header data -->
   <!-- END  Battery #2 data-->
   <!-- BEGIN  Battery #3 data-->
   <!-- BEGIN  Battery #3 Header data -->
   <hdr_node name="B3IDMIN (Battery #3 ID-MIN ADC Value)" byte_cnt="2" offset="03A9">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B3IDMAX (Battery #3 ID-MAX ADC Value)" byte_cnt="2" offset="03AB">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B3TYPE (Battery #3 Type)" byte_cnt="1" offset="03AD">
     <data>02</data>
   </hdr_node>
   <hdr_node name="B3CAP (Battery #3 Capacity (mAH))" byte_cnt="2" offset="03AE">
     <data>05DC</data>
   </hdr_node>
   <hdr_node name="B3VMAX lower(Battery #3 Max Voltage (mV))" byte_cnt="1" offset="03B0">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B3VMAX upper(Battery #3 Max Voltage (mV))" byte_cnt="1" offset="03B1">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B3LOWBATTLS (Battery #3 Low Setting-LOWBATTDET)" byte_cnt="1" offset="03B2">
     <data>C7</data>
   </hdr_node>
   <hdr_node name="B3SAFE (Battery #3 Safe Voltage/Current Limit-CHRSAFELMT)" byte_cnt="1" offset="03B3">
     <data>40</data>
   </hdr_node>
   <!-- END  Battery #3 Header data -->
   <!-- BEGIN  Battery #3 Temp Range 4 data -->
   <hdr_node name="B3T4UL (Temp Range 4 UL - Degrees C Upper Bound +60C)" byte_cnt="2" offset="03B4">
     <data>003C</data>
   </hdr_node>
   <hdr_node name="B3T4PR (Temp Range 4 Pack Resistance)" byte_cnt="1" offset="03B6">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T4FCV lower(Temp Range 4 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="03B7">
     <data>04</data>
   </hdr_node>
   <hdr_node name="B3T4FCV upper(Temp Range 4 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="03B8">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B3T4FCI (Temp Range 4 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="03B9">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B3T4MCVSTART lower(Temp Range 4 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03BB">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T4MCVSTART upper(Temp Range 4 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03BC">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T4MCVSTOP (Temp Range 4 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="03BD">
     <data>0FC8</data>
   </hdr_node>
   <hdr_node name="B3T4MCI lower(Temp Range 4 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="03BF">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B3T4MCI upper(Temp Range 4 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="03C0">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #3 Temp Range 4 data -->
   <!-- BEGIN  Battery #3 Temp Range 3 data -->
   <hdr_node name="B3T3UL (Temp Range 3 UL - Degrees C Upper Bound +45C)" byte_cnt="2" offset="03C1">
     <data>002D</data>
   </hdr_node>
   <hdr_node name="B3T3PR (Temp Range 3 Pack Resistance)" byte_cnt="1" offset="03C3">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T3FCV lower(Temp Range 3 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="03C4">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B3T3FCV upper(Temp Range 3 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="03C5">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B3T3FCI (Temp Range 3 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="03C6">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B3T3MCVSTART lower(Temp Range 3 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03C8">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T3MCVSTART upper(Temp Range 3 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03C9">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T3MCVSTOP (Temp Range 3 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="03CA">
     <data>102C</data>
   </hdr_node>
   <hdr_node name="B3T3MCI lower(Temp Range 3 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="03CC">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B3T3MCI upper(Temp Range 3 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="03CD">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #3 Temp Range 3 data -->
   <!-- BEGIN  Battery #3 Temp Range 2 data -->
   <hdr_node name="B3T2UL (Temp Range 2 UL - Degrees C Upper Bound +10C)" byte_cnt="2" offset="03CE">
     <data>000A</data>
   </hdr_node>
   <hdr_node name="B3T2PR (Temp Range 2 Pack Resistance)" byte_cnt="1" offset="03D0">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T2FCV lower(Temp Range 2 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="03D1">
     <data>04</data>
   </hdr_node>
   <hdr_node name="B3T2FCV upper(Temp Range 2 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="03D2">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B3T2FCI (Temp Range 2 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="03D3">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B3T2MCVSTART lower(Temp Range 2 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03D5">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T2MCVSTART upper(Temp Range 2 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03D6">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T2MCVSTOP (Temp Range 2 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="03D7">
     <data>0FC8</data>
   </hdr_node>
   <hdr_node name="B3T2MCI lower(Temp Range 2 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="03D9">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B3T2MCI upper(Temp Range 2 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="03DA">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #3 Temp Range 2 data -->
   <!-- BEGIN  Battery #3 Temp Range 1 data -->
   <hdr_node name="B3T1UL (Temp Range 1 UL - Degrees C Upper Bound +0C)" byte_cnt="2" offset="03DB">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B3T1PR (Temp Range 1 Pack Resistance)" byte_cnt="1" offset="03DD">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T1FCV lower(Temp Range 1 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="03DE">
     <data>64</data>
   </hdr_node>
   <hdr_node name="B3T1FCV upper(Temp Range 1 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="03DF">
     <data>0F</data>
   </hdr_node>
   <hdr_node name="B3T1FCI (Temp Range 1 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="03E0">
     <data>0190</data>
   </hdr_node>
   <hdr_node name="B3T1MCVSTART lower(Temp Range 1 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03E2">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T1MCVSTART upper(Temp Range 1 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03E3">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B3T1MCVSTOP (Temp Range 1 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="03E4">
     <data>0F64</data>
   </hdr_node>
   <hdr_node name="B3T1MCI lower(Temp Range 1 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="03E6">
     <data>90</data>
   </hdr_node>
   <hdr_node name="B3T1MCI upper(Temp Range 1 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="03E7">
     <data>01</data>
   </hdr_node>
   <!-- END    Battery #3 Temp Range 1 data -->
   <hdr_node name="B3T1LL (Temp Range 1 LL - Degrees C Lower Bound -10C)" byte_cnt="2" offset="03E8">
     <data>800A</data>
   </hdr_node>
   <!-- END    Battery #3 Header data -->
   <!-- END  Battery #3 data-->
   <!-- BEGIN  Battery #4 data-->
   <!-- BEGIN  Battery #4 Header data -->
   <hdr_node name="B4IDMIN (Battery #4 ID-MIN ADC Value)" byte_cnt="2" offset="03EA">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B4IDMAX - (Battery #4 ID-MAX ADC Value)" byte_cnt="2" offset="03EC">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B4TYPE (Battery #4 Type)" byte_cnt="1" offset="03EE">
     <data>02</data>
   </hdr_node>
   <hdr_node name="B4CAP (Battery #4 Capacity (mAH))" byte_cnt="2" offset="03EF">
     <data>05DC</data>
   </hdr_node>
   <hdr_node name="B4VMAX lower(Battery #4 Max Voltage (mV))" byte_cnt="1" offset="03F1">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B4VMAX upper(Battery #4 Max Voltage (mV))" byte_cnt="1" offset="03F2">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B4LOWBATTLS (Battery #4 Low Setting-LOWBATTDET)" byte_cnt="1" offset="03F3">
     <data>C7</data>
   </hdr_node>
   <hdr_node name="B4SAFE (Battery #4 Safe Voltage/Current Limit-CHRSAFELMT)" byte_cnt="1" offset="03F4">
     <data>40</data>
   </hdr_node>
   <!-- END  Battery #4 Header data -->
   <!-- BEGIN  Battery #4 Temp Range 4 data -->
   <hdr_node name="B4T4UL (Temp Range 4 UL - Degrees C Upper Bound +60C)" byte_cnt="2" offset="03F5">
     <data>003C</data>
   </hdr_node>
   <hdr_node name="B4T4PR (Temp Range 4 Pack Resistance)" byte_cnt="1" offset="03F7">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T4FCV lower(Temp Range 4 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="03F8">
     <data>04</data>
   </hdr_node>
   <hdr_node name="B4T4FCV upper(Temp Range 4 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="03F9">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B4T4FCI (Temp Range 4 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="03FA">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B4T4MCVSTART lower(Temp Range 4 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03FC">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T4MCVSTART upper(Temp Range 4 Maintenance Charge Voltage LT)" byte_cnt="1" offset="03FD">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T4MCVSTOP (Temp Range 4 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="03FE">
     <data>0FC8</data>
   </hdr_node>
   <hdr_node name="B4T4MCI lower(Temp Range 4 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0400">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B4T4MCI upper(Temp Range 4 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0401">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #4 Temp Range 4 data -->
   <!-- BEGIN  Battery #4 Temp Range 3 data -->
   <hdr_node name="B4T3UL (Temp Range 3 UL - Degrees C Upper Bound +45C)" byte_cnt="2" offset="0402">
     <data>002D</data>
   </hdr_node>
   <hdr_node name="B4T3PR (Temp Range 3 Pack Resistance)" byte_cnt="1" offset="0404">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T3FCV lower(Temp Range 3 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0405">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B4T3FCV upper(Temp Range 3 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0406">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B4T3FCI (Temp Range 3 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0407">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B4T3MCVSTART lower(Temp Range 3 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0409">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T3MCVSTART upper(Temp Range 3 Maintenance Charge Voltage LT)" byte_cnt="1" offset="040A">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T3MCVSTOP (Temp Range 3 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="040B">
     <data>102C</data>
   </hdr_node>
   <hdr_node name="B4T3MCI lower(Temp Range 3 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="040D">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B4T3MCI upper(Temp Range 3 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="040E">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #4 Temp Range 3 data -->
   <!-- BEGIN  Battery #4 Temp Range 2 data -->
   <hdr_node name="B4T2UL (Temp Range 2 UL - Degrees C Upper Bound +10C)" byte_cnt="2" offset="040F">
     <data>000A</data>
   </hdr_node>
   <hdr_node name="B4T2PR (Temp Range 2 Pack Resistance)" byte_cnt="1" offset="0411">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T2FCV lower(Temp Range 2 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0412">
     <data>04</data>
   </hdr_node>
   <hdr_node name="B4T2FCV upper(Temp Range 2 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0413">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B4T2FCI (Temp Range 2 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0414">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B4T2MCVSTART lower(Temp Range 2 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0416">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T2MCVSTART upper(Temp Range 2 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0417">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T2MCVSTOP (Temp Range 2 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="0418">
     <data>0FC8</data>
   </hdr_node>
   <hdr_node name="B4T2MCI lower(Temp Range 2 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="041A">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B4T2MCI upper(Temp Range 2 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="041B">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #4 Temp Range 2 data -->
   <!-- BEGIN  Battery #4 Temp Range 1 data -->
   <hdr_node name="B4T1UL (Temp Range 1 UL - Degrees C Upper Bound +0C)" byte_cnt="2" offset="041C">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B4T1PR (Temp Range 1 Pack Resistance)" byte_cnt="1" offset="041E">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T1FCV lower(Temp Range 1 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="041F">
     <data>64</data>
   </hdr_node>
   <hdr_node name="B4T1FCV upper(Temp Range 1 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0420">
     <data>0F</data>
   </hdr_node>
   <hdr_node name="B4T1FCI (Temp Range 1 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0421">
     <data>0190</data>
   </hdr_node>
   <hdr_node name="B4T1MCVSTART lower(Temp Range 1 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0423">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T1MCVSTART upper(Temp Range 1 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0424">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B4T1MCVSTOP (Temp Range 1 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="0425">
     <data>0F64</data>
   </hdr_node>
   <hdr_node name="B4T1MCI lower(Temp Range 1 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0427">
     <data>90</data>
   </hdr_node>
   <hdr_node name="B4T1MCI upper(Temp Range 1 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0428">
     <data>01</data>
   </hdr_node>
   <!-- END    Battery #4 Temp Range 1 data -->
   <hdr_node name="B4T1LL (Temp Range 1 LL - Degrees C Lower Bound -10C)" byte_cnt="2" offset="0429">
     <data>800A</data>
   </hdr_node>
   <!-- END    Battery #4 Header data -->
   <!-- END    Battery #4 data-->

   <!-- BEGIN  Battery #5 data-->
   <!-- BEGIN  Battery #5 Header data -->
   <hdr_node name="B5IDMIN (Battery #5 ID-MIN ADC Value)" byte_cnt="2" offset="042B">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B5IDMAX (Battery #5 ID-MAX ADC Value)" byte_cnt="2" offset="042D">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B5TYPE (Battery #5 Type)" byte_cnt="1" offset="042F">
     <data>02</data>
   </hdr_node>
   <hdr_node name="B5CAP (Battery #5 Capacity (mAH))" byte_cnt="2" offset="0430">
     <data>05DC</data>
   </hdr_node>
   <hdr_node name="B5VMAX lower(Battery #5 Max Voltage (mV))" byte_cnt="1" offset="0432">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B5VMAX upper(Battery #5 Max Voltage (mV))" byte_cnt="1" offset="0433">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B5LOWBATTLS (Battery #5 Low Setting-LOWBATTDET)" byte_cnt="1" offset="0434">
     <data>C7</data>
   </hdr_node>
   <hdr_node name="B5SAFE (Battery #5 Safe Voltage/Current Limit-CHRSAFELMT)" byte_cnt="1" offset="0435">
     <data>40</data>
   </hdr_node>
   <!-- END  Battery #5 Header data -->
   <!-- BEGIN  Battery #5 Temp Range 4 data -->
   <hdr_node name="B5T4UL (Temp Range 4 UL - Degrees C Upper Bound +60C)" byte_cnt="2" offset="0436">
     <data>003C</data>
   </hdr_node>
   <hdr_node name="B5T4PR (Temp Range 4 Pack Resistance)" byte_cnt="1" offset="0438">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T4FCV lower(Temp Range 4 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0439">
     <data>04</data>
   </hdr_node>
   <hdr_node name="B5T4FCV upper(Temp Range 4 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="043A">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B5T4FCI (Temp Range 4 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="043B">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B5T4MCVSTART lower(Temp Range 4 Maintenance Charge Voltage LT)" byte_cnt="1" offset="043D">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T4MCVSTART upper(Temp Range 4 Maintenance Charge Voltage LT)" byte_cnt="1" offset="043E">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T4MCVSTOP (Temp Range 4 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="043F">
     <data>0FC8</data>
   </hdr_node>
   <hdr_node name="B5T4MCI lower(Temp Range 4 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0441">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B5T4MCI upper(Temp Range 4 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0442">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #5 Temp Range 4 data -->
   <!-- BEGIN  Battery #5 Temp Range 3 data -->
   <hdr_node name="B5T3UL (Temp Range 3 UL - Degrees C Upper Bound +45C)" byte_cnt="2" offset="0443">
     <data>002D</data>
   </hdr_node>
   <hdr_node name="B5T3PR (Temp Range 3 Pack Resistance)" byte_cnt="1" offset="0445">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T3FCV lower(Temp Range 3 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0446">
     <data>68</data>
   </hdr_node>
   <hdr_node name="B5T3FCV upper(Temp Range 3 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0447">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B5T3FCI (Temp Range 3 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0448">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B5T3MCVSTART lower(Temp Range 3 Maintenance Charge Voltage LT)" byte_cnt="1" offset="044A">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T3MCVSTART upper(Temp Range 3 Maintenance Charge Voltage LT)" byte_cnt="1" offset="044B">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T3MCVSTOP (Temp Range 3 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="044C">
     <data>102C</data>
   </hdr_node>
   <hdr_node name="B5T3MCI lower(Temp Range 3 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="044E">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B5T3MCI upper(Temp Range 3 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="044F">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #5 Temp Range 3 data -->
   <!-- BEGIN  Battery #5 Temp Range 2 data -->
   <hdr_node name="B5T2UL (Temp Range 2 UL - Degrees C Upper Bound +10C)" byte_cnt="2" offset="0450">
     <data>000A</data>
   </hdr_node>
   <hdr_node name="B5T2PR (Temp Range 2 Pack Resistance)" byte_cnt="1" offset="0452">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T2FCV lower(Temp Range 2 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0453">
     <data>04</data>
   </hdr_node>
   <hdr_node name="B5T2FCV upper(Temp Range 2 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0454">
     <data>10</data>
   </hdr_node>
   <hdr_node name="B5T2FCI (Temp Range 2 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0455">
     <data>03B6</data>
   </hdr_node>
   <hdr_node name="B5T2MCVSTART lower(Temp Range 2 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0457">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T2MCVSTART lower(Temp Range 2 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0458">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T2MCVSTOP (Temp Range 2 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="0459">
     <data>0FC8</data>
   </hdr_node>
   <hdr_node name="B5T2MCI lower(Temp Range 2 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="045B">
     <data>B6</data>
   </hdr_node>
   <hdr_node name="B5T2MCI upper(Temp Range 2 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="045C">
     <data>03</data>
   </hdr_node>
   <!-- END    Battery #5 Temp Range 2 data -->
   <!-- BEGIN  Battery #5 Temp Range 1 data -->
   <hdr_node name="B5T1UL (Temp Range 1 UL - Degrees C Upper Bound +0C)" byte_cnt="2" offset="045D">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="B5T1PR (Temp Range 1 Pack Resistance)" byte_cnt="1" offset="045F">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T1FCV lower(Temp Range 1 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0460">
     <data>64</data>
   </hdr_node>
   <hdr_node name="B5T1FCV upper(Temp Range 1 Fast Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="1" offset="0461">
     <data>0F</data>
   </hdr_node>
   <hdr_node name="B5T1FCI (Temp Range 1 Fast Charge Current Limit-CHRCCURRENT)" byte_cnt="2" offset="0462">
     <data>0190</data>
   </hdr_node>
   <hdr_node name="B5T1MCVSTART lower(Temp Range 1 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0464">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T1MCVSTART upper(Temp Range 1 Maintenance Charge Voltage LT)" byte_cnt="1" offset="0465">
     <data>00</data>
   </hdr_node>
   <hdr_node name="B5T1MCVSTOP (Temp Range 1 Maintenance Charge Voltage UT-CHRCVOLTAGE)" byte_cnt="2" offset="0466">
     <data>0F64</data>
   </hdr_node>
   <hdr_node name="B5T1MCI lower(Temp Range 1 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0468">
     <data>90</data>
   </hdr_node>
   <hdr_node name="B5T1MCI upper(Temp Range 1 Maintenance Charge Current Limit-CHRCCURRENT" byte_cnt="1" offset="0469">
     <data>01</data>
   </hdr_node>
   <!-- END    Battery #5 Temp Range 1 data -->
   <hdr_node name="B5T1LL (Temp Range 1 LL - Degrees C Lower Bound -10C)" byte_cnt="2" offset="046A">
     <data>800A</data>
   </hdr_node>
   <!-- END    Battery #5 data-->

   <!-- BEGIN  BCU #1 data -->
   <hdr_node name="BCU1BCLIM12" byte_cnt="1" offset="046C">
     <data>25</data>
   </hdr_node>
   <hdr_node name="BCU1BTIMELIM12" byte_cnt="1" offset="046D">
     <data>80</data>
   </hdr_node>
   <hdr_node name="BCU1BTIMELIM3" byte_cnt="1" offset="046E">
     <data>05</data>
   </hdr_node>
   <hdr_node name="BCU1BTIMEDB" byte_cnt="1" offset="046F">
     <data>10</data>
   </hdr_node>
   <hdr_node name="BCU1BCFGOUTS" byte_cnt="1" offset="0470">
     <data>18</data>
   </hdr_node>
   <hdr_node name="BCU1BCFGACTS" byte_cnt="1" offset="0471">
     <data>4F</data>
   </hdr_node>
   <hdr_node name="BCU1CAPACITYTHRES" byte_cnt="1" offset="0472">
     <data>14</data>
   </hdr_node>
   <!-- END    BCU #1 data -->
   <!-- BEGIN  BCU #2 data -->
   <hdr_node name="BCU2BCLIM12" byte_cnt="1" offset="0473">
     <data>23</data>
   </hdr_node>
   <hdr_node name="BCU2BTIMELIM12" byte_cnt="1" offset="0474">
     <data>50</data>
   </hdr_node>
   <hdr_node name="BCU2BTIMELIM3" byte_cnt="1" offset="0475">
     <data>04</data>
   </hdr_node>
   <hdr_node name="BCU2BTIMEDB" byte_cnt="1" offset="0476">
     <data>00</data>
   </hdr_node>
   <hdr_node name="BCU2BCFGOUTS" byte_cnt="1" offset="0477">
     <data>1F</data>
   </hdr_node>
   <hdr_node name="BCU2BCFGACTS" byte_cnt="1" offset="0478">
     <data>4F</data>
   </hdr_node>
   <hdr_node name="BCU2CAPACITYTHRES" byte_cnt="1" offset="0479">
     <data>05</data>
   </hdr_node>
   <!-- END    BCU #2 data -->
   <!-- BEGIN  BCU #3 data -->
   <hdr_node name="BCU3BCLIM12" byte_cnt="1" offset="047A">
     <data>12</data>
   </hdr_node>
   <hdr_node name="BCU3BTIMELIM12" byte_cnt="1" offset="047B">
     <data>00</data>
   </hdr_node>
   <hdr_node name="BCU3BTIMELIM3" byte_cnt="1" offset="047C">
     <data>01</data>
   </hdr_node>
   <hdr_node name="BCU3BTIMEDB" byte_cnt="1" offset="047D">
     <data>00</data>
   </hdr_node>
   <hdr_node name="BCU3BCFGOUTS" byte_cnt="1" offset="047E">
     <data>1F</data>
   </hdr_node>
   <hdr_node name="BCU3BCFGACTS" byte_cnt="1" offset="047F">
     <data>7F</data>
   </hdr_node>
   <!-- END    BCU #3 data -->

   <!-- BEGIN SBCT - Reserved -->
   <hdr_node name="SBCT Reserved 0x0480" byte_cnt="20" offset="0480">
     <data>0000000000000000000000000000000000000000</data>
   </hdr_node>
   <!-- END  SBCT - Reserved -->
   <!-- END  SBCT - Supportted Battery Characteristics Table-->

   <!-- BEGIN Reserved -->
   <hdr_node name="Intel Reserved 0x0494" byte_cnt="876" offset="0494">
     <data>000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <!-- END  Reserved -->
   <!-- BEGIN  Security Keys-->
   <hdr_node name="Security Public Key 0" byte_cnt="256" offset="0800">
     <data>FEEDFACE000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000FACEFEEDFEEDFACE000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000FACEFEED</data>
   </hdr_node>
   <hdr_node name="Security Public Key 1" byte_cnt="256" offset="0900">
     <data>FEEDFACE000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000FACEFEEDFEEDFACE000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000FACEFEED</data>
   </hdr_node>
   <hdr_node name="Security Public Key 2" byte_cnt="256" offset="0A00">
     <data>FEEDFACE000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000FACEFEEDFEEDFACE000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000FACEFEED</data>
   </hdr_node>
   <hdr_node name="Security Public Key 3" byte_cnt="256" offset="0B00">
     <data>FEEDFACE000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000FACEFEEDFEEDFACE000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000FACEFEED</data>
   </hdr_node>
   <hdr_node name="Security Public Key 4" byte_cnt="256" offset="0C00">
     <data>FEEDFACE000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000FACEFEEDFEEDFACE000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000FACEFEED</data>
   </hdr_node>
   <!-- END  Security Keys-->
   <!-- BEGIN  Reserved-->
   <hdr_node name="Reserved 0xD00" byte_cnt="12288" offset="0D00">
     <data>000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <hdr_node name="Reserved 0x3D00" byte_cnt="12288" offset="3D00">
     <data>000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <hdr_node name="Reserved 0x6D00" byte_cnt="12288" offset="6D00">
     <data>000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <hdr_node name="Reserved 0x9D00" byte_cnt="12288" offset="9D00">
     <data>000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <hdr_node name="Reserved 0xCD00" byte_cnt="12544" offset="CD00">
     <data>00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <!-- END Reserved-->
 </header>
 <!-- END SMIP -->
 <!-- UMIP -->
 <!-- BEGIN UMIP Header -->
 <header title="Unsigned Master Image Profile (UMIP) Header">
   <hdr_node name="Signature" byte_cnt="04" offset="0000" readonly="true">
     <data>UMIP</data>
   </hdr_node>
   <hdr_node name="Header Size" byte_cnt="02" offset="0004" readonly="true">
     <data>4000</data>
   </hdr_node>
   <hdr_node name="Header Revision" byte_cnt="01" offset="0006">
     <data>01</data>
   </hdr_node>
   <hdr_node name="Header CheckSum" byte_cnt="01" offset="0007" readonly="true">
     <data>01</data>
   </hdr_node>
   <!-- END UMIP Header -->
   <!-- BEGIN Checksum Table -->
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0008" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0009" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="000C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="000D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0010" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0011" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0014" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0015" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0018" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0019" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="001C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="001D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0020" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0021" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0024" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0025" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0028" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0029" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="002C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="002D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0030" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0031" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0034" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0035" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0038" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0039" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="003C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="003D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0040" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0041" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0044" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0045" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0048" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0049" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="004C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="004D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0050" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0051" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0054" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0055" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0058" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0059" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="005C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="005D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0060" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0061" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0064" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0065" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0068" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0069" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="006C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="006D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0070" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0071" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0074" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0075" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0078" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0079" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="007C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="007D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0080" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0081" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0084" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0085" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0088" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0089" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="008C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="008D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0090" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0091" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0094" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0095" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0098" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0099" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="009C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="009D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00A0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00A1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00A4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00A5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00A8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00A9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00AC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00AD" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00B0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00B1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00B4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00B5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00B8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00B9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00BC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00BD" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00C0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00C1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00C4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00C5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00C8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00C9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00CC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00CD" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00D0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00D1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00D4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00D5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00D8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00D9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00DC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00DD" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00E0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00E1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00E4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00E5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00E8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00E9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00EC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00ED" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00F0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00F1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00F4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00F5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00F8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00F9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="00FC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="00FD" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0100" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0101" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0104" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0105" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0108" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0109" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="010C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="010D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0110" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0111" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0114" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0115" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0118" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0119" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="011C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="011D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0120" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0121" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0124" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0125" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0128" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0129" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="012C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="012D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0130" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0131" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0134" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0135" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0138" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0139" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="013C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="013D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0140" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0141" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0144" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0145" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0148" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0149" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="014C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="014D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0150" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0151" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0154" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0155" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0158" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0159" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="015C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="015D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0160" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0161" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0164" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0165" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0168" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0169" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="016C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="016D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0170" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0171" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0174" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0175" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0178" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0179" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="017C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="017D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0180" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0181" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0184" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0185" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0188" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0189" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="018C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="018D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0190" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0191" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0194" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0195" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="0198" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="0199" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="019C" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="019D" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01A0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01A1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01A4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01A5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01A8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01A9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01AC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01AD" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01B0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01B1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01B4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01B5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01B8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01B9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01BC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01BD" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01C0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01C1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01C4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01C5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01C8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01C9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01CC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01CD" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01D0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01D1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01D4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01D5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01D8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01D9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01DC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01DD" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01E0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01E1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01E4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01E5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01E8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01E9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01EC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01ED" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01F0" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01F1" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01F4" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01F5" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01F8" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01F9" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Calculated Checksum " byte_cnt="1" offset="01FC" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved " byte_cnt="3" offset="01FD" readonly="true">
     <data>000000</data>
   </hdr_node>
   <!--END UMIP CHECKSUM -->
   <hdr_node name="Reserved 0x200" byte_cnt="256" offset="0200" readonly="true">
     <data>00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <hdr_node name="Reserved 0x300" byte_cnt="256" offset="0300" readonly="true">
     <data>00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <!-- END Reserved-->
   <!-- BEGIN Software Revocation Table -->
   <hdr_node name="USB Host Enable" byte_cnt="01" offset="0400">
     <data>3F</data>
   </hdr_node>
   <hdr_node name="Reserved" byte_cnt="03" offset="0401" readonly="true">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="HVM Hooks" byte_cnt="04" offset="0404">
     <data>00000000</data>
   </hdr_node>
   <!-- END Software Revocation Table -->
   <!-- BEGIN Reserved -->
   <hdr_node name="Reserved 0x0408" byte_cnt="248" offset="0408" readonly="true">
     <data>0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <!-- END Reserved -->
   <!-- BEGIN Versions-->
   <hdr_node name="Intel Ucode Minor Version" byte_cnt="04" offset="0500">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="IA FW Minor Version" byte_cnt="04" offset="0504">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="OS Minor Version" byte_cnt="04" offset="0508">
     <data>00000000</data>
   </hdr_node>
   <!-- END Versions -->
   <!-- BEGIN Reserved -->
   <hdr_node name="Reserved 0x50C" byte_cnt="256" offset="050C" readonly="true">
     <data>00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <!-- END   Reserved -->
   <!-- BEGIN PTI Hooks -->
   <hdr_node name="PTI Hooks" byte_cnt="64" offset="060C">
     <data>00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <!-- END PTI Hooks-->
   <!-- BEGIN Reserved -->
   <hdr_node name="Reserved" byte_cnt="436" offset="064C">
     <data>0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <!-- END    Reserved -->

   <!-- BEGIN Energy Management Data block - EMDB (1KB) 0x800 thru 0xBFF-->
   <!-- BEGIN Header -->
   <hdr_node name="Revision (Major, Minor)" byte_cnt="02" offset="0800">
     <data>0010</data>
   </hdr_node>
   <hdr_node name="Battery Discharge UMIP Size in bytes" byte_cnt="02" offset="0802">
     <data>0400</data>
   </hdr_node>
   <hdr_node name="Reserved (chksum placeholder)" byte_cnt="02" offset="0804">
     <data>0000</data>
   </hdr_node>
   <!-- END   Header -->

   <!-- BEGIN Reference Fuel Guage Table -->
   <hdr_node name="Revision (Major, Minor)" byte_cnt="02" offset="0806">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="Table Name" byte_cnt="04" offset="0808">
     <data>4D415831</data>
   </hdr_node>
   <hdr_node name="Battery ID lower" byte_cnt="04" offset="080C">
     <data>4B303030</data>
   </hdr_node>
   <hdr_node name="Battery ID upper" byte_cnt="04" offset="0810">
     <data>494E4344</data>
   </hdr_node>
   <hdr_node name="Size (in bytes including header)" byte_cnt="02" offset="0814">
     <data>0090</data>
   </hdr_node>
   <hdr_node name="FG Table Type (0=Charge vs VBatt; 1=RBatt vs Temp; 2=Vendor Specific)" byte_cnt="01" offset="0816">
     <data>02</data>
   </hdr_node>
   <hdr_node name="Reserved 0817" byte_cnt="01" offset="0817">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Data1" byte_cnt="04" offset="0818">
     <data>16210056</data>
   </hdr_node>
   <hdr_node name="Data2" byte_cnt="04" offset="081C">
     <data>0C1C0350</data>
   </hdr_node>
   <hdr_node name="Data3" byte_cnt="04" offset="0820">
     <data>0C1C0000</data>
   </hdr_node>
   <hdr_node name="Data4" byte_cnt="04" offset="0824">
     <data>01400000</data>
   </hdr_node>
   <hdr_node name="Data5" byte_cnt="04" offset="0828">
     <data>2D510C1C</data>
   </hdr_node>
   <hdr_node name="Data6" byte_cnt="04" offset="082C">
     <data>22100001</data>
   </hdr_node>
   <hdr_node name="Data7" byte_cnt="04" offset="0830">
     <data>87A40076</data>
   </hdr_node>
   <hdr_node name="Data8" byte_cnt="04" offset="0834">
     <data>A250506B</data>
   </hdr_node>
   <hdr_node name="Data9" byte_cnt="04" offset="0838">
     <data>B800B720</data>
   </hdr_node>
   <hdr_node name="Data10" byte_cnt="04" offset="083C">
     <data>B920B880</data>
   </hdr_node>
   <hdr_node name="Data11" byte_cnt="04" offset="0840">
     <data>BA60BA00</data>
   </hdr_node>
   <hdr_node name="Data12" byte_cnt="04" offset="0844">
     <data>BCF0BBF0</data>
   </hdr_node>
   <hdr_node name="Data13" byte_cnt="04" offset="0848">
     <data>C060BE50</data>
   </hdr_node>
   <hdr_node name="Data14" byte_cnt="04" offset="084C">
     <data>C520C2D0</data>
   </hdr_node>
   <hdr_node name="Data15" byte_cnt="04" offset="0850">
     <data>CA00C750</data>
   </hdr_node>
   <hdr_node name="Data16" byte_cnt="04" offset="0854">
     <data>0120D090</data>
   </hdr_node>
   <hdr_node name="Data17" byte_cnt="04" offset="0858">
     <data>04701C80</data>
   </hdr_node>
   <hdr_node name="Data18" byte_cnt="04" offset="085C">
     <data>01000440</data>
   </hdr_node>
   <hdr_node name="Data19" byte_cnt="04" offset="0860">
     <data>09605500</data>
   </hdr_node>
   <hdr_node name="Data20" byte_cnt="04" offset="0864">
     <data>22502410</data>
   </hdr_node>
   <hdr_node name="Data21" byte_cnt="04" offset="0868">
     <data>0BD015F0</data>
   </hdr_node>
   <hdr_node name="Data22" byte_cnt="04" offset="086C">
     <data>0B000D00</data>
   </hdr_node>
   <hdr_node name="Data23" byte_cnt="04" offset="0870">
     <data>08A00BB0</data>
   </hdr_node>
   <hdr_node name="Data24" byte_cnt="04" offset="0874">
     <data>010008A0</data>
   </hdr_node>
   <hdr_node name="Data25" byte_cnt="04" offset="0878">
     <data>01000100</data>
   </hdr_node>
   <hdr_node name="Data26" byte_cnt="04" offset="087C">
     <data>01000100</data>
   </hdr_node>
   <hdr_node name="Data27" byte_cnt="04" offset="0880">
     <data>01000100</data>
   </hdr_node>
   <hdr_node name="Data28" byte_cnt="04" offset="0884">
     <data>01000100</data>
   </hdr_node>
   <hdr_node name="Data29" byte_cnt="04" offset="0888">
     <data>01000100</data>
   </hdr_node>
   <hdr_node name="Data30" byte_cnt="04" offset="088C">
     <data>01000100</data>
   </hdr_node>
   <hdr_node name="Data31" byte_cnt="04" offset="0890">
     <data>01000100</data>
   </hdr_node>
   <hdr_node name="Data32" byte_cnt="02" offset="0894">
     <data>0100</data>
   </hdr_node>
   <!-- END    Reference Fuel Guage Table -->

   <!-- BEGIN Configuration Data -->
   <hdr_node name="Total Number of Tables" byte_cnt="01" offset="0896">
     <data>05</data>
   </hdr_node>

   <!-- BEGIN Fuel Guage Table (Batt 1) -->
   <hdr_node name="Revision (Major, Minor)" byte_cnt="02" offset="0897">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="Table Name" byte_cnt="04" offset="0899">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Battery ID lower" byte_cnt="04" offset="089D">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Battery ID upper" byte_cnt="04" offset="08A1">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Size (in bytes including header)" byte_cnt="02" offset="08A5">
     <data>0090</data>
   </hdr_node>
   <hdr_node name="FG Table Type (0=Charge vs VBatt; 1=RBatt vs Temp; 2=Vendor Specific)" byte_cnt="01" offset="08A7">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved 08A8" byte_cnt="01" offset="08A8">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Data1" byte_cnt="03" offset="08A9">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Data2" byte_cnt="04" offset="08AC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data3" byte_cnt="04" offset="08B0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data4" byte_cnt="04" offset="08B4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data5" byte_cnt="04" offset="08B8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data6" byte_cnt="04" offset="08BC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data7" byte_cnt="04" offset="08C0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data8" byte_cnt="04" offset="08C4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data9" byte_cnt="04" offset="08C8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data10" byte_cnt="04" offset="08CC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data11" byte_cnt="04" offset="08D0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data12" byte_cnt="04" offset="08D4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data13" byte_cnt="04" offset="08D8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data14" byte_cnt="04" offset="08DC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data15" byte_cnt="04" offset="08E0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data16" byte_cnt="04" offset="08E4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data17" byte_cnt="04" offset="08E8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data18" byte_cnt="04" offset="08EC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data19" byte_cnt="04" offset="08F0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data20" byte_cnt="04" offset="08F4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data21" byte_cnt="04" offset="08F8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data22" byte_cnt="04" offset="08FC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data23" byte_cnt="04" offset="0900">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data24" byte_cnt="04" offset="0904">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data25" byte_cnt="04" offset="0908">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data26" byte_cnt="04" offset="090C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data27" byte_cnt="04" offset="0910">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data28" byte_cnt="04" offset="0914">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data29" byte_cnt="04" offset="0918">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data30" byte_cnt="04" offset="091C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data31" byte_cnt="04" offset="0920">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data32" byte_cnt="03" offset="0924">
     <data>000000</data>
   </hdr_node>
   <!-- END   Fuel Guage Table (Batt 1) -->

   <!-- BEGIN Fuel Guage Table (Batt 2) -->
   <hdr_node name="Revision (Major, Minor)" byte_cnt="02" offset="0927">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="Table Name" byte_cnt="04" offset="0929">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Battery ID lower" byte_cnt="04" offset="092D">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Battery ID upper" byte_cnt="04" offset="0931">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Size (in bytes including header)" byte_cnt="02" offset="0935">
     <data>0090</data>
   </hdr_node>
   <hdr_node name="FG Table Type (0=Charge vs VBatt; 1=RBatt vs Temp; 2=Vendor Specific)" byte_cnt="01" offset="0937">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved 0938" byte_cnt="01" offset="0938">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Data1" byte_cnt="03" offset="0939">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Data2" byte_cnt="04" offset="093C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data3" byte_cnt="04" offset="0940">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data4" byte_cnt="04" offset="0944">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data5" byte_cnt="04" offset="0948">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data6" byte_cnt="04" offset="094C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data7" byte_cnt="04" offset="0950">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data8" byte_cnt="04" offset="0954">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data9" byte_cnt="04" offset="0958">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data10" byte_cnt="04" offset="095C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data11" byte_cnt="04" offset="0960">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data12" byte_cnt="04" offset="0964">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data13" byte_cnt="04" offset="0968">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data14" byte_cnt="04" offset="096C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data15" byte_cnt="04" offset="0970">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data16" byte_cnt="04" offset="0974">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data17" byte_cnt="04" offset="0978">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data18" byte_cnt="04" offset="097C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data19" byte_cnt="04" offset="0980">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data20" byte_cnt="04" offset="0984">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data21" byte_cnt="04" offset="0988">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data22" byte_cnt="04" offset="098C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data23" byte_cnt="04" offset="0990">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data24" byte_cnt="04" offset="0994">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data25" byte_cnt="04" offset="0998">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data26" byte_cnt="04" offset="099C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data27" byte_cnt="04" offset="09A0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data28" byte_cnt="04" offset="09A4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data29" byte_cnt="04" offset="09A8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data30" byte_cnt="04" offset="09AC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data31" byte_cnt="04" offset="09B0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data32" byte_cnt="03" offset="09B4">
     <data>000000</data>
   </hdr_node>
   <!-- END   Fuel Guage Table (Batt 2) -->

   <!-- BEGIN Fuel Guage Table (Batt 3) -->
   <hdr_node name="Revision (Major, Minor)" byte_cnt="02" offset="09B7">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="Table Name" byte_cnt="04" offset="09B9">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Battery ID lower" byte_cnt="04" offset="09BD">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Battery ID upper" byte_cnt="04" offset="09C1">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Size (in bytes including header)" byte_cnt="02" offset="09C5">
     <data>0090</data>
   </hdr_node>
   <hdr_node name="FG Table Type (0=Charge vs VBatt; 1=RBatt vs Temp; 2=Vendor Specific)" byte_cnt="01" offset="09C7">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved 09C8" byte_cnt="01" offset="09C8">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Data1" byte_cnt="03" offset="09C9">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Data2" byte_cnt="04" offset="09CC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data3" byte_cnt="04" offset="09D0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data4" byte_cnt="04" offset="09D4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data5" byte_cnt="04" offset="09D8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data6" byte_cnt="04" offset="09DC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data7" byte_cnt="04" offset="09E0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data8" byte_cnt="04" offset="09E4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data9" byte_cnt="04" offset="09E8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data10" byte_cnt="04" offset="09EC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data11" byte_cnt="04" offset="09F0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data12" byte_cnt="04" offset="09F4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data13" byte_cnt="04" offset="09F8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data14" byte_cnt="04" offset="09FC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data15" byte_cnt="04" offset="0A00">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data16" byte_cnt="04" offset="0A04">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data17" byte_cnt="04" offset="0A08">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data18" byte_cnt="04" offset="0A0C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data19" byte_cnt="04" offset="0A10">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data20" byte_cnt="04" offset="0A14">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data21" byte_cnt="04" offset="0A18">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data22" byte_cnt="04" offset="0A1C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data23" byte_cnt="04" offset="0A20">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data24" byte_cnt="04" offset="0A24">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data25" byte_cnt="04" offset="0A28">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data26" byte_cnt="04" offset="0A2C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data27" byte_cnt="04" offset="0A30">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data28" byte_cnt="04" offset="0A34">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data29" byte_cnt="04" offset="0A38">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data30" byte_cnt="04" offset="0A3C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data31" byte_cnt="04" offset="0A40">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data32" byte_cnt="03" offset="0A44">
     <data>000000</data>
   </hdr_node>
   <!-- END   Fuel Guage Table (Batt 3) -->

   <!-- BEGIN Fuel Guage Table (Batt 4) -->
   <hdr_node name="Revision (Major, Minor)" byte_cnt="02" offset="0A47">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="Table Name" byte_cnt="04" offset="0A49">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Battery ID lower" byte_cnt="04" offset="0A4D">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Battery ID upper" byte_cnt="04" offset="0A51">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Size (in bytes including header)" byte_cnt="02" offset="0A55">
     <data>0090</data>
   </hdr_node>
   <hdr_node name="FG Table Type (0=Charge vs VBatt; 1=RBatt vs Temp; 2=Vendor Specific)" byte_cnt="01" offset="0A57">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved 0A58" byte_cnt="01" offset="0A58">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Data1" byte_cnt="03" offset="0A59">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Data2" byte_cnt="04" offset="0A5C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data3" byte_cnt="04" offset="0A60">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data4" byte_cnt="04" offset="0A64">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data5" byte_cnt="04" offset="0A68">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data6" byte_cnt="04" offset="0A6C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data7" byte_cnt="04" offset="0A70">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data8" byte_cnt="04" offset="0A74">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data9" byte_cnt="04" offset="0A78">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data10" byte_cnt="04" offset="0A7C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data11" byte_cnt="04" offset="0A80">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data12" byte_cnt="04" offset="0A84">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data13" byte_cnt="04" offset="0A88">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data14" byte_cnt="04" offset="0A8C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data15" byte_cnt="04" offset="0A90">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data16" byte_cnt="04" offset="0A94">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data17" byte_cnt="04" offset="0A98">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data18" byte_cnt="04" offset="0A9C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data19" byte_cnt="04" offset="0AA0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data20" byte_cnt="04" offset="0AA4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data21" byte_cnt="04" offset="0AA8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data22" byte_cnt="04" offset="0AAC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data23" byte_cnt="04" offset="0AB0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data24" byte_cnt="04" offset="0AB4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data25" byte_cnt="04" offset="0AB8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data26" byte_cnt="04" offset="0ABC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data27" byte_cnt="04" offset="0AC0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data28" byte_cnt="04" offset="0AC4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data29" byte_cnt="04" offset="0AC8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data30" byte_cnt="04" offset="0ACC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data31" byte_cnt="04" offset="0AD0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data32" byte_cnt="03" offset="0AD4">
     <data>000000</data>
   </hdr_node>
   <!-- END   Fuel Guage Table (Batt 4) -->

   <!-- BEGIN Fuel Guage Table (Batt 5) -->
   <hdr_node name="Revision (Major, Minor)" byte_cnt="02" offset="0AD7">
     <data>0000</data>
   </hdr_node>
   <hdr_node name="Table Name" byte_cnt="04" offset="0AD9">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Battery ID lower" byte_cnt="04" offset="0ADD">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Battery ID upper" byte_cnt="04" offset="0AE1">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Size (in bytes including header)" byte_cnt="02" offset="0AE5">
     <data>0090</data>
   </hdr_node>
   <hdr_node name="FG Table Type (0=Charge vs VBatt; 1=RBatt vs Temp; 2=Vendor Specific)" byte_cnt="01" offset="0AE7">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Reserved 0AE8" byte_cnt="01" offset="0AE8">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Data1" byte_cnt="03" offset="0AE9">
     <data>000000</data>
   </hdr_node>
   <hdr_node name="Data2" byte_cnt="04" offset="0AEC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data3" byte_cnt="04" offset="0AF0">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data4" byte_cnt="04" offset="0AF4">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data5" byte_cnt="04" offset="0AF8">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data6" byte_cnt="04" offset="0AFC">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data7" byte_cnt="04" offset="0B00">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data8" byte_cnt="04" offset="0B04">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data9" byte_cnt="04" offset="0B08">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data10" byte_cnt="04" offset="0B0C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data11" byte_cnt="04" offset="0B10">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data12" byte_cnt="04" offset="0B14">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data13" byte_cnt="04" offset="0B18">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data14" byte_cnt="04" offset="0B1C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data15" byte_cnt="04" offset="0B20">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data16" byte_cnt="04" offset="0B24">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data17" byte_cnt="04" offset="0B28">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data18" byte_cnt="04" offset="0B2C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data19" byte_cnt="04" offset="0B30">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data20" byte_cnt="04" offset="0B34">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data21" byte_cnt="04" offset="0B38">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data22" byte_cnt="04" offset="0B3C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data23" byte_cnt="04" offset="0B40">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data24" byte_cnt="04" offset="0B44">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data25" byte_cnt="04" offset="0B48">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data26" byte_cnt="04" offset="0B4C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data27" byte_cnt="04" offset="0B50">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data28" byte_cnt="04" offset="0B54">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data29" byte_cnt="04" offset="0B58">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data30" byte_cnt="04" offset="0B5C">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data31" byte_cnt="04" offset="0B60">
     <data>00000000</data>
   </hdr_node>
   <hdr_node name="Data32" byte_cnt="03" offset="0B64">
     <data>000000</data>
   </hdr_node>
   <!-- END   Fuel Guage Table (Batt 5) -->
   <!-- END   Configuration Data -->

   <!-- BEGIN Reserved -->
   <hdr_node name="Reserved" byte_cnt="153" offset="0B67">
     <data>000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <!-- END   Reserved -->
   <!-- END    Energy Management Data block - EMDB (1KB) 0x800 thru 0xBFF-->

   <!-- BEGIN Reserved -->
   <hdr_node name="Reserved" byte_cnt="9216" offset="0C00">
     <data>000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <hdr_node name="Reserved" byte_cnt="12288" offset="3000">
     <data>000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <!-- END Reserved -->
   <!-- BEGIN Security Firmware -->
   <hdr_node name="Intel Security Firmware" byte_cnt="8192" offset="6000">
     <data>0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <hdr_node name="OEM Security Firmware" byte_cnt="12288" offset="8000">
     <data>000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <hdr_node name="OEM Security Firmware" byte_cnt="12288" offset="B000">
     <data>000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <hdr_node name="OEM Security Firmware" byte_cnt="8192" offset="E000">
     <data>0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
   <!-- END Security Firmware -->
 </header>
 <!-- END UMIP -->
 <!-- FIP -->
 <header title="Firmware Image Profile (FIP) Header">
   <hdr_node name="Signature" byte_cnt="04" offset="0000" readonly="true">
     <data>$$FIP</data>
   </hdr_node>
   <hdr_node name="Header Size" byte_cnt="01" offset="0004" readonly="true">
     <data>24</data>
   </hdr_node>
   <hdr_node name="Header Minor Revision" byte_cnt="01" offset="0005">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Header Major Revision" byte_cnt="01" offset="0006">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Header Checksum" byte_cnt="01" offset="0007" readonly="true">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Intel RESERVED 08" byte_cnt="01" offset="0008">
     <data>00</data>
   </hdr_node>
   <hdr_node name="IA-32 Firmware Minor Revision" byte_cnt="01" offset="0009">
     <data>00</data>
   </hdr_node>
   <hdr_node name="IA-32 Firmware Major Revision" byte_cnt="01" offset="000A">
     <data>00</data>
   </hdr_node>
   <hdr_node name="IA-32 Firmware Checksum" byte_cnt="01" offset="000B">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Intel RESERVED" byte_cnt="01" offset="000C">
     <data>00</data>
   </hdr_node>
   <hdr_node name="P-unit Microcode Minor Revision" byte_cnt="01" offset="000D">
     <data>00</data>
   </hdr_node>
   <hdr_node name="P-unit Microcode Major Revision" byte_cnt="01" offset="000E">
     <data>00</data>
   </hdr_node>
   <hdr_node name="P-unit Microcode Checksum" byte_cnt="01" offset="000F">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Intel RESERVED 10" byte_cnt="01" offset="0010">
     <data>00</data>
   </hdr_node>
   <hdr_node name="OEM Hooks Minor Revision" byte_cnt="01" offset="0011">
     <data>00</data>
   </hdr_node>
   <hdr_node name="OEM Hooks Major Revision" byte_cnt="01" offset="0012">
     <data>00</data>
   </hdr_node>
   <hdr_node name="OEM Hooks Checksum" byte_cnt="01" offset="0013">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Intel RESERVED 14" byte_cnt="01" offset="0014">
     <data>00</data>
   </hdr_node>
   <hdr_node name="IA-32 FW Minor Supplemental" byte_cnt="01" offset="0015">
     <data>00</data>
   </hdr_node>
   <hdr_node name="IA-32 FW Major Supplemental" byte_cnt="01" offset="0016">
     <data>00</data>
   </hdr_node>
   <hdr_node name="IA-32 FW Checksum Supplemental" byte_cnt="01" offset="0017">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Intel RESERVED 18" byte_cnt="01" offset="0018">
     <data>00</data>
   </hdr_node>
   <hdr_node name="SCU Microcode Minor Revision" byte_cnt="01" offset="0019">
     <data>00</data>
   </hdr_node>
   <hdr_node name="SCU Microcode Major Revision" byte_cnt="01" offset="001A">
     <data>00</data>
   </hdr_node>
   <hdr_node name="SCU Microcode Checksum" byte_cnt="01" offset="001B">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Intel RESERVED 1C" byte_cnt="01" offset="001C">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Chaabi iCache FW Minor Revision" byte_cnt="01" offset="001D">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Chaabi iCache FW Major Revision" byte_cnt="01" offset="001E">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Chaabi iCache FW Checksum" byte_cnt="01" offset="001F">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Intel RESERVED 20" byte_cnt="01" offset="0020">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Chaabi Resident FW Minor Revision" byte_cnt="01" offset="0021">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Chaabi Resident FW Major Revision" byte_cnt="01" offset="0022">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Chaabi Resident FW Checksum" byte_cnt="01" offset="0023">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Intel RESERVED 24" byte_cnt="01" offset="0024">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Chaabi EXT FW Revision" byte_cnt="01" offset="0025">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Chaabi EXT FW Major Revision" byte_cnt="01" offset="0026">
     <data>00</data>
   </hdr_node>
   <hdr_node name="Chaabi EXT FW Checksum" byte_cnt="01" offset="0027">
     <data>00</data>
   </hdr_node>
   <hdr_node name="IFWI Minor Revision" byte_cnt="01" offset="0028">
     <data>12</data>
   </hdr_node>
   <hdr_node name="IFWI Major Revision" byte_cnt="01" offset="0029">
     <data>B0</data>
   </hdr_node>
   <hdr_node name="Intel RESERVED 2A" byte_cnt="102" offset="002A">
     <data>000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000</data>
   </hdr_node>
 </header>
<nand_options>
<redundancy>3</redundancy>
<pages_per_block>64</pages_per_block>
<page_size>4</page_size>
<channel_config>0</channel_config>
<ecc_type>1</ecc_type>
<spare_area>218</spare_area>
<number_of_versions>4</number_of_versions>
</nand_options>
<osip_header>
<signature>609439524</signature>
<!-- The string "$$OS$$"-->
<intel_reserved>0</intel_reserved>
<header_minor_revision>0</header_minor_revision>
<header_major_revision>1</header_major_revision>
<header_checksum>14</header_checksum>
<number_of_pointers>$num_images</number_of_pointers>
<number_of_images>1</number_of_images>
<header_size>80</header_size>
$osimage_lines
</osip_header>
<spi_overrides/>
<psct_data/>
<sct_data/>
</platform>
"""


def get_penwell_xml(images, is_signed, stepping):
    sub_dict = {
            "osimage_lines" : get_os_image_xml(images, is_signed),
            "stepping" : stepping,
            "num_images" : len(images)
        }
    return Template(penwell_xml_template).substitute(sub_dict)

if __name__ == "__main__":
    main()
