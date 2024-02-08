import subprocess
import filecmp
import os

NUM_OF_TESTS = 7

def run_c_program(run):
    args_list = [".\sim.exe"]
    args_list.append("test_txt_files/" + str(run) +"_cfg.txt")
    args_list.append("test_txt_files/" + str(run) + "_memin.txt")
    args_list.append("test_txt_files/" + str(run) + "_regout.txt")
    args_list.append("test_txt_files/" + str(run) + "_traceinst.txt")
    args_list.append("test_txt_files/" + str(run) + "_tracecdb.txt")
    subprocess.run(args_list, shell=True)

def compare_output(run, file_name):

    generated_output_path = "test_txt_files/" + str(run) + "_" + file_name + ".txt"
    predicted_output_path = "test_txt_files/"+ str(run) + "_" + file_name + "_expected.txt"

    try:
        comparison_result = filecmp.cmp(generated_output_path, predicted_output_path)
        if not comparison_result:
            print(f"Output file {file_name} do not match for run {run}.")
        #os.remove(generated_output_path) #Optional to delete output files after execution and comparison
    except FileNotFoundError as e:
        if e.filename == generated_output_path:
            print(f"Simulation output file {generated_output_path} not found.")
        elif e.filename == predicted_output_path:
            print(f"Predicted output file {predicted_output_path} not found.")

if __name__ == "__main__":
    for run in range(1, NUM_OF_TESTS + 1):
        run_c_program(run)
        compare_output(run, "regout")
        compare_output(run, "traceinst")
        compare_output(run, "tracecdb")
        print(f"Test {run} out of {NUM_OF_TESTS} completed")