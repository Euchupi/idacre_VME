import pymongo
from pymongo import MongoClient
from bson.objectid import ObjectId
import os


# the run_mode data is stored at the options collection . 
client = MongoClient("mongodb://192.168.1.88:27017")
db = client['daq']
collection = db['options']



run_mode = {
	# the run_mode name must be unique 
    "name": "default_firmware_settings",
    "user": "root",
    "description": "V1725_number_0",
    "detector" : "tpc",
	"detectors": {"detector" : "tpc",
				  "hostname" : "RelicsDAQ_reader_0",
				  "RelicsDAQ_reader_0": "CylinderTPC",
				  "RelicsDAQ": "tpc" , 
				  "active" : "true",
				  "stop_after" : "600"}, # second
    "mongo_uri": "mongodb://192.168.1.88:27017/admin",
    "mongo_database": "daq",
    "run_start":0,
    "strax_chunk_overlap": 500000000,
    "strax_header_size": 31,
    "strax_output_path": "/home/data/tpc",
    "strax_chunk_length": 5000000000,
    "strax_fragment_length": 220,
    "baseline_dac_mode": "fit",
    "baseline_value": 16000,
    "firmware_version": 4.22,
    "boards":
    [
        {"crate": 0, "link": 0, "board": 165,
            "vme_address": "0", "type": "V1730", "host": "RelicsDAQ_reader_0"},
    ],
    "registers" : [
		{
			"comment" : "board reset register",
			"board" : -1,
			"reg" : "EF24",
			"val" : "0"
		},
	],
    "channels":{"165":[0, 1, 2, 3, 4, 5, 6, 7]},
}

if collection.find_one({"name": run_mode['name']}) is not None:
    print("Please provide a unique name!")

try:
    collection.insert_one(run_mode)
except Exception as e:
    print("Insert failed. Maybe your JSON is bad. Error follows:")
    print(e)


# we need to make sure some convention is satisfied
detector_from_path = run_mode['strax_output_path'].split('/')
detector_from_path=[i for i in detector_from_path if i !='']

if detector_from_path[0]!='home':
	print('Not conventional path. Should be /home/data/<detector>')
	exit(0)

if run_mode['detector'] not in detector_from_path:
	print('Inconsistent save path and detector name. They should be the same')
	exit(0)



print("New runmode has been added as a document of collection 'options'")