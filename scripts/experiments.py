import scan
import subprocess
import time
from prettytable import PrettyTable
from collections import namedtuple

similarity_measures = ["identity 1 x 1 1;"]

# input name, input num frames, reference frame, target name, num target frames
maps = [("Combat_Hit", 1, 0, "Block_1H_Rtrn", 1)]

directories = [
	"../Sprites/FullyAnimated/Chest_OrderArmor_Heavy/",
	"../Sprites/FullyAnimated/Chest_OrderArmor_Light/",
	"../Sprites/FullyAnimated/Template_Muscular_Black/",
	"../Sprites/PartiallyAnimated/Chest_FamilyMedium/", # mostly complete
	"../Sprites/FullyAnimated/Template_Muscular_White/",
	"../Sprites/PartiallyAnimated/Chest_Hunter/",
	"../Sprites/PartiallyAnimated/Chest_LeatherArmor_Torn/",
	"../Sprites/PartiallyAnimated/Chest_OrderArmor_Medium/", # missing many anis
	"../Sprites/PartiallyAnimated/Chest_Linen/",
	]

zone_map = "-i \"../Sprites/Demo/Mask_Combat_Hit.png\" -t \"../Sprites/Demo/Mask_Block_1H_Rtrn.png\""

#Point = namedtuple('Point', 'x y')

reference_sets = [[0], [1], [0,1,2], [0,1,2,6,7], [2]]
test_sets = [[3],[3],[3],[3], [4]]

def run_experiment(map_args, ref_set, test_set, similarity_measure,
	zone_map=None):
	# create map
	zone_map_cmd = ""
	if zone_map:
		zone_map_cmd = "-i {} -t {} -z"
	dirs = [directories[i] for i in ref_set]
	map_pairs, _ = scan.find_reference_pairs(dirs, map_args[0], map_args[3])
	param_list = scan.make_param_list(map_pairs)
	command = "AniGen create {} {} -n {} -m {} -r {} -s \"{}\" -o \"test.map\" -j 4".format(
		zone_map_cmd,
		param_list, 
		map_args[1], 
		map_args[4], 
		map_args[2],
		similarity_measure)

	start = time.time()
	subprocess.run(command, check=True)
	end = time.time()
	
	# apply map
	dirs = [directories[i] for i in test_set]
	map_pairs, _ = scan.find_reference_pairs(dirs, map_args[0], map_args[3])
	command = "AniGen apply -i {} -t test.map -o sprite".format(map_pairs[0][0])
	subprocess.run(command, check=True)
	
	# evaluate result
	command = "AniGen evaluate -i \"{}\" -t sprite_test.png".format(map_pairs[0][1])
	result = subprocess.run(command, check=True, capture_output=True, text=True)

	return list(map(float, result.stdout.split(","))), end-start

results_table = PrettyTable()
results_table.field_names = ["name", "inputs", "0-error", "1-error"]
for map_args in maps:
	for ref_set,test_set in zip(reference_sets, test_sets):
		for similarity_measure in similarity_measures:
			err, duration = run_experiment(map_args, ref_set, test_set, similarity_measure)
			results_table.add_row(["armor", len(ref_set), err[0], err[1]])

print(results_table)