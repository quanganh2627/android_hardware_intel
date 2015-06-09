import os.path
import sys
import xml.dom.minidom as minidom
import struct
import array

def getTiSwConfig(xml, confFile):
	batt_id_offset = 8;#First element is batt_id of 8 bytes
	numBytes = 0;
	itr = 0;
	f = open(confFile, 'wb')
        conf = minidom.parse(xml)
        node = conf.documentElement

	TiSwData = conf.getElementsByTagName("tifg_config")
	if TiSwData.length == 0:
		f.close()
		return
	TiSw = []
	for datum in TiSwData:
		elements = datum.childNodes
		for element in elements:
			TiSw.append(element)
		for top in TiSw:
			nodes = top.childNodes
			for node in nodes:
				if node.parentNode.nodeName == "battid":
						f.write(str(node.data))
						numBytes += 8
				else:
					f.write(struct.pack("I", int(node.data, 16)))
					numBytes += 4
	f.close()

	f = open(confFile, 'rb+')
        ##Compute checksum and write to file
        f.seek(0)
        tmp = f.read()
        cksum = sum(array.array('B', tmp))
        f.seek(0)
        f.write(struct.pack("H", cksum))
        f.close()


if __name__ == "__main__":
	global confFile
	basepath = os.path.dirname(__file__)
	xmlFile = str(sys.argv[1])
	confFile = str(sys.argv[2])

	getTiSwConfig(xmlFile, confFile)
