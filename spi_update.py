import subprocess
import time
import sys
from multiprocessing import Process
def execute_adb_command(command):
    try:
        result = subprocess.run(command, shell=True, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        print(result.stdout.decode())
        return True
    except subprocess.CalledProcessError as e:
        print(e.stderr.decode())
        return False

def reboot_device():
    result1=execute_adb_command("adb -s 2ce88c3f26c9d2ce reboot")
    if result1:
        print("仪表重启成功")
    else:
        print("仪表重启失败")
    time.sleep(1)

    result2=execute_adb_command("adb -s 192.168.3.68 reboot")
    if result2:
        print("MP5重启成功")
    else:
        print("MP5重启失败")
    if result1 and result2:
        print("设备正在重启中，等待重新连接......")
        return True
    else:
        print("设备重启失败，等待重新尝试")
        return False

def connect_device(address):
    max_attempts = 3
    connection_attempts = 0
    while connection_attempts < max_attempts:
        print(f"正在尝试连接设备 {address}...")
        if execute_adb_command(f"adb connect {address}"):
            print(f"connect to {address} success")
            return True
        else:
            connection_attempts += 1
            print(f"设备连接失败{connection_attempts}次，待重新连接...")
            time.sleep(1)
    return False

adb_commands = [
    'adb -s 192.168.3.68 shell input tap 500 760',
    'adb -s 192.168.3.68 shell input tap 765 1213',
    'adb -s 192.168.3.68 shell input swipe 100 330 100 100 15',
    'adb -s 192.168.3.68 shell input swipe 100 330 100 100 15',
    'adb -s 192.168.3.68 shell input tap 100 1082',
    'adb -s 192.168.3.68 shell input tap 400 180',
    'adb -s 192.168.3.68 shell input tap 500 650',
    'adb -s 192.168.3.68 shell input tap 420 640',
]

def countdown(t):
    while t:
        mins, secs = divmod(t, 60)
        time_str = '{:02d}:{:02d}'.format(mins, secs)
        print(time_str, end='后自动重启设备\r')
        time.sleep(1)
        t -= 1


def run_as_process(target_func, *args, **kwargs):
    process = Process(target=target_func, args=args, kwargs=kwargs)
    process.start()
    process.join()  

delay_time = 2
def main():
    err_counts=0
    address = '192.168.3.68'
    address2 = '2ce88c3f26c9d2ce'
    start_time = time.time()  # 记录脚本开始时间
    time.sleep(5)
    while True:
        # if err_counts>2:
        #     err_counts=0
        #     print("设备连接失败次数过多,自动退出,需手动重启中控仪表设备\n")
        #     sys.exit(1)
        print("--------------设备启动中------------")
        time.sleep(20)
        print("中控界面初始化中......")
        time.sleep(50)
        if connect_device(address):
            print("准备执行ADB命令......")
            time.sleep(3)
            all_commands_success = True
            for cmd in adb_commands:
                print(f"{cmd}")
                if not execute_adb_command(cmd):
                    all_commands_success = False
                    print("命令执行失败，等待重试......")
                    err_counts+=1
                    if connect_device(address):
                        reboot_device()
                        break; 
                    break;  
                time.sleep(delay_time) 
            if all_commands_success:
                print("ADB命令执行成功,即将进入升级界面，如中控未进入升级界面，请重启脚本和中控仪表")
                print("----------USB升级中,请勿操作----------")
                print("等待升级程序退出，自动重启设备...")
                countdown(540) 
                print("\n--------------------------------------------------------")  
                reboot_device() 
        else:
            print("设备连接失败超过3次,重启设备...")
            reboot_device()
if __name__ == "__main__":
    run_as_process(main)