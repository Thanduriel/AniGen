import subprocess
import re
import os

# input name, input num frames, reference frame, target name, num target frames
map_params = ("Input", 1, 0, "Walk", 8)

alt_directories = [
	"../Sprites/Input/",
]

parts = [
	"Belt", "Boots", "Chest", "Legs", "Shoulder"
]

zone_map_path = "../Sprites/ZoneMap/"

def makeZoneMapName(part, level, ani):
	if(level >= 3):
		return ("{}ZoneMap_Male_{}_ExtraDetailed_Input.png".format(zone_map_path,part),
			"{}ZoneMap_Male_{}_{}_ExtraDetailed.png".format(zone_map_path,part, ani))
	else:
		return ("{}ZoneMap_Male_{}_Input_{}.png".format(zone_map_path, part, level),
			"{}ZoneMap_Male_{}_{}_{}".format(zone_map_path, part, ani, level))

similarity_measures = ["identity 1 x 1 1;",
					   "equality 3 x 3 1 1 1; 1 1 1; 1 1 1;",
					   "equality 3 x 3 1 1 1; 1 3 1; 1 1 1;",
					   "equality 3 x 3 0.845 0.529 0.732; 0.561 0.754 0.367; 0.758 0.141 0.243;",
					   "equalityrotinv 3 x 3 1 1 1; 1 3 1; 1 1 1;",
					   "equalityrotinv 3 x 3 0.90553009 0.51973339 0.56880356 0.73786717 0.76955676 0.0523793 0.60405608 0.30422786 0.39515804;",
					   "mseoptim 3 x 3",
					   "mseoptim 5 x 5"
]

generation_input = "-i ../Sprites/Input/Male_Skin_Input.png -t ../Sprites/Input/Male_Skin_Run.png"

test_input_path = "../Sprites/Input/Armor/"

def makeReferenceAnis():
	# create maps
	for part in parts:
		out_name = "{}_Walk.map".format(part)
		if os.path.isfile(out_name):
			continue
		zone_map_name = makeZoneMapName(part, 3, "Walk")
		if not os.path.isfile(zone_map_name[0]) or not os.path.isfile(zone_map_name[1]):
			continue
		zone_map_cmd = "-i {} -t {} -z".format(zone_map_name[0], zone_map_name[1])
		command = "AniGen create {} {} -n {} -m {} -r {} -s \"{}\" -o \"{}\" --crop {} -j 4".format(
			zone_map_cmd,
			generation_input,
			1,
			8,
			0,
			similarity_measures[1],
			out_name,
			2
		)
		subprocess.run(command, check=True, capture_output=False)

	reg_ex = re.compile('Input_(\w*)_(\d*).png')
	for f in os.listdir(test_input_path):
		m = re.match(reg_ex, f)
		if m:
			part = m.groups()[0]
			num = m.groups()[1]

			map_name = "{}_Walk.map".format(part)
			if not os.path.isfile(map_name):
				continue

			command = "AniGen apply -i {} -n {} -m {} -r {} -t {} -o {}".format(
				test_input_path + f,
				1, 
				8, 
				0,
				map_name,
				"Output_{}".format(num)
			)
			subprocess.run(command, check=True, capture_output=False)

makeReferenceAnis()