using System;
using System.Collections.Generic;
using System.Windows.Forms;

namespace network.loki.lokinet.win32.ui
{
    static class Program
    {
        public static OperatingSystem os_id = Environment.OSVersion;
        public static PlatformID platform = os_id.Platform;
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new main_frame());
            try
            {
                main_frame.lokiNetDaemon.Kill();
            }
            catch
            { }
        }
    }
}
