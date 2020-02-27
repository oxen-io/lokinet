import pyllarp

def main():
  hive = pyllarp.RouterHive()
  hive.StartAll()
  hive.StopAll()

if __name__ == '__main__':
  main()