using network.loki.lokinet.win32.ui.Properties;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;

namespace network.loki.lokinet.win32.ui
{

    class LogDumper
    {
        private const string LogFolderName = "logs";
        private const string LogFileName = "lokinet.log";
        private string tmp;
        private string LogPath;

        public LogDumper(string text_dump)
        {
            tmp = text_dump;
        }

        public void setText(string text)
        {
            tmp = text;
        }
        public string getLogPath()
        {
            return LogPath;
        }

        public void CreateLog(string path)
        {
            var logFolderPath = Path.Combine(path, LogFolderName);
            if (!Directory.Exists(logFolderPath))
                Directory.CreateDirectory(logFolderPath);

            var logFilePath = Path.Combine(logFolderPath, LogFileName);
            LogPath = logFilePath;

            Rotate(logFilePath);

            using (var sw = File.AppendText(logFilePath))
            {
                sw.WriteLine(tmp);
            }
        }

        private void Rotate(string filePath)
        {
            if (!File.Exists(filePath))
                return;

            var fileInfo = new FileInfo(filePath);

            var fileTime = DateTime.Now.ToString("dd-MM-yy__h-m-s");
            var rotatedPath = filePath.Replace(".log", $".{fileTime}.log");
            File.Move(filePath, rotatedPath);
        }
    }
    
}
