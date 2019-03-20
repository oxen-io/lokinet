using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using System.Windows.Forms;

namespace network.loki.lokinet.win32.ui
{
    public partial class main_frame : Form
    {
        public static Process lokiNetDaemon = new Process();
        public static bool isConnected;
        public static string logText;

        void UpdateUI(string text)
        {
            this.Invoke(new MethodInvoker(delegate () { lokinetd_fd1.AppendText(text); }));
        }

        public main_frame()
        {
            InitializeComponent();
            StatusLabel.Text = "Disconnected";
            var build = ((AssemblyInformationalVersionAttribute)Assembly
  .GetAssembly(typeof(main_frame))
  .GetCustomAttributes(typeof(AssemblyInformationalVersionAttribute), false)[0])
  .InformationalVersion;
            UIVersionLabel.Text = String.Format("LokiNET version {0}", build);
            lokinetd_fd1.Text = string.Empty;
            logText = string.Empty;
        }

        private void btnConfigProfile_Click(object sender, EventArgs e)
        {
            //MessageBox.Show("not implemented yet", "error", MessageBoxButtons.OK, MessageBoxIcon.Asterisk);
            UserSettingsForm f = new UserSettingsForm();
            f.ShowDialog();
            f.Dispose();
        }

        private void btnConnect_Click(object sender, EventArgs e)
        {
            string lokinetExeString;

            if (Program.platform == PlatformID.Win32NT)
                lokinetExeString = String.Format("{0}\\lokinet.exe", Directory.GetCurrentDirectory());
            else
                lokinetExeString = String.Format("{0}/lokinet", Directory.GetCurrentDirectory());

            lokiNetDaemon.StartInfo.UseShellExecute = false;
            lokiNetDaemon.StartInfo.RedirectStandardOutput = true;
            //lokiNetDaemon.EnableRaisingEvents = true;
            lokiNetDaemon.StartInfo.CreateNoWindow = true;
            lokiNetDaemon.StartInfo.FileName = lokinetExeString;
            lokiNetDaemon.OutputDataReceived += new DataReceivedEventHandler((s, ev) =>
            {
                if (!string.IsNullOrEmpty(ev.Data))
                {
                    UpdateUI(ev.Data + Environment.NewLine);
                }
            });
            lokiNetDaemon.Start();
            lokiNetDaemon.BeginOutputReadLine();
            btnConnect.Enabled = false;
            StatusLabel.Text = "Connected";
            isConnected = true;
            NotificationTrayIcon.Text = "LokiNET - connected";
            btnDrop.Enabled = true;
        }

        private void btnDrop_Click(object sender, EventArgs e)
        {
            lokiNetDaemon.CancelOutputRead();
            lokiNetDaemon.Kill();
            btnConnect.Enabled = true;
            btnDrop.Enabled = false;
            StatusLabel.Text = "Disconnected";
            NotificationTrayIcon.Text = "LokiNET - disconnected";
            isConnected = false;
            logText = lokinetd_fd1.Text;
            lokinetd_fd1.Text = string.Empty;
        }

        private void lokinetd_fd1_TextChanged(object sender, EventArgs e)
        {
            lokinetd_fd1.ScrollToCaret();
        }

        private void btnHide_Click(object sender, EventArgs e)
        {
            Hide();
            if (isConnected)
                NotificationTrayIcon.ShowBalloonTip(5, "LokiNET", "Currently connected.", ToolTipIcon.Info);
            else
                NotificationTrayIcon.ShowBalloonTip(5, "LokiNET", "Currently disconnected.", ToolTipIcon.Info);
        }

        private void NotificationTrayIcon_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            Show();
        }

        private void btnAbout_Click(object sender, EventArgs e)
        {
            AboutBox a = new AboutBox();
            a.ShowDialog();
            a.Dispose();
        }
    }
}
