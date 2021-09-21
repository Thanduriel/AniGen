import scan
import subprocess
import time

similarity_measures = ["identity 1 x 1 1;"]

# input name, input num frames, reference frame, target name, num target frames
maps = [("Combat_Hit", 1, 0, "Block_1H_Rtrn", 1)]

directories = [
	"../Sprites/FullyAnimated/Chest_OrderArmor_Heavy/",
	"../Sprites/FullyAnimated/Chest_OrderArmor_Light/",
	"../Sprites/FullyAnimated/Template_Muscular_Black",
	"../Sprites/PartiallyAnimated/Chest_FamilyMedium"]

zone_map = "-i \"../Sprites/Demo/Mask_Combat_Hit.png\" -t \"../Sprites/Demo/Mask_Block_1H_Rtrn.png\""

# index for directories
reference_sets = [[0], [1], [0,1,2]]
test_sets = [[3],[3],[3]]

for map_args in maps:
	for ref_set,test_set in zip(reference_sets, test_sets):
		for similarity_measure in similarity_measures:
			dirs = [directories[i] for i in ref_set]
			map_pairs, _ = scan.find_reference_pairs(dirs, map_args[0], map_args[3])
			param_list = scan.make_param_list(map_pairs)
			command = "AniGen create {} -n {} -m {} -r {} -s \"{}\" -o \"test.map\" -j 4".format(
				param_list, 
				map_args[1], 
				map_args[4], 
				map_args[2],
				similarity_measure)

			start = time.time()
			
			subprocess.run(command, check=True)

			dirs = [directories[i] for i in test_set]
			map_pairs, _ = scan.find_reference_pairs(dirs, map_args[0], map_args[3])
			command = "AniGen apply -i {} -t test.map -o sprite".format(map_pairs[0][0])
			subprocess.run(command, check=True)
			
			command = "AniGen difference -i \"{}\" -t sprite_test.png".format(map_pairs[0][0])
			result = subprocess.run(command, check=True, capture_output=True, text=True)
			print(result.stdout)

			end = time.time()
		#	print(end-start)