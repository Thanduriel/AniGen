import subprocess
import re
import os
import scan
import experiments

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
			"{}ZoneMap_Male_{}_{}_{}.png".format(zone_map_path, part, ani, level))

similarity_measures = ["identity 1 x 1 1;",
					   "equality 3 x 3 1 1 1; 1 1 1; 1 1 1;",
					   "equality 3 x 3 1 1 1; 1 3 1; 1 1 1;",
					   "equality 3 x 3 0.845 0.529 0.732; 0.561 0.754 0.367; 0.758 0.141 0.243;",
					   "equalityrotinv 3 x 3 1 1 1; 1 1 1; 1 1 1;",
					   "equalityrotinv 3 x 3 1 1 1; 1 3 1; 1 1 1;",
					   "equalityrotinv 3 x 3 0.90553009 0.51973339 0.56880356; 0.73786717 0.76955676 0.0523793; 0.60405608 0.30422786 0.39515804;",
					   "equalityrotinv 3 x 3 0.0 1.0 0.0; 1.0 5.0 1.0; 0.0 1.0 0.0;",
					   "mseoptim 3 x 3",
					   "mseoptim 5 x 5"
]

generation_input = [("../Sprites/Input/Male_Skin_Input.png", "../Sprites/Input/Male_Skin_Run.png")]

reg_ex = re.compile('Input_(\w*)_(\d*).png')
test_input_path = "../Sprites/Input/Armor/"
test_output_path = "../Sprites/Output/Armor/"

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
		inputs_args = scan.make_arg_list(generation_input)
		command = "AniGen create {} {} -n {} -m {} -r {} -s \"{}\" -o \"{}\" --crop {} -j 4".format(
			zone_map_cmd,
			inputs_args,
			1,
			8,
			0,
			similarity_measures[1],
			out_name,
			2
		)
		subprocess.run(command, check=True, capture_output=False)

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

def main():
	#makeReferenceAnis()
	test_sets  = {}
	for part in parts:
		test_sets[part] = []
	for f in os.listdir(test_input_path):
		m = re.match(reg_ex, f)
		if m:
			part = m.groups()[0]
			num = m.groups()[1]
			out_name = "Output_{}_{}_Walk.png".format(num, part)
			test_sets[part].append((test_input_path + f, test_output_path + out_name))

	part = "Chest"
	for zone_map_level in range(0,4):
		runs = []
		for similarity in similarity_measures:
			runs.append(experiments.Experiment(similarity, 
				map_params, generation_input, test_sets[part], 
				similarity, makeZoneMapName(part, zone_map_level, "Walk")))

		print("zone map detail level: {}".format(zone_map_level))
		print(experiments.make_table(runs))

if __name__ == "__main__":
	main()