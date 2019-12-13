using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

namespace network.loki.lokinet.win32.ui
{
    public partial class UserSettingsForm : Form
    {
        public UserSettingsForm()
        {
            InitializeComponent();
            if (Program.platform == PlatformID.Win32NT)
                config_path = Environment.ExpandEnvironmentVariables("%APPDATA%\\.lokinet");
            else
                config_path = Environment.ExpandEnvironmentVariables("%HOME%/.lokinet");
        }

        private string config_path;
        private LogDumper ld;

        private void btnOK_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void btnBoot_Click(object sender, EventArgs e)
        {
            dlgBootstrap b = new dlgBootstrap();
            b.ShowDialog();
            b.Dispose();
        }

        private void btnDumpLog_Click(object sender, EventArgs e)
        {
            if (main_frame.isConnected)
                MessageBox.Show("Cannot dump log when client is running.", "Warning", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
            else
            {
                if (main_frame.logText == string.Empty)
                {
                    MessageBox.Show("Log is empty", "Warning", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
                    return;
                }
                if (ld == null)
                    ld = new LogDumper(main_frame.logText);
                else
                    ld.setText(main_frame.logText);

                ld.CreateLog(config_path);
                MessageBox.Show(string.Format("Wrote log to {0}, previous log rotated", ld.getLogPath()), "LokiNET", MessageBoxButtons.OK, MessageBoxIcon.Information);
                main_frame.logText = string.Empty;
            }
        }

        private void btnVSettings_Click(object sender, EventArgs e)
        {
            VisualSettings v = new VisualSettings();
            v.ShowDialog();
            v.Dispose();
        }

        private void BtnEditCfg_Click(object sender, EventArgs e)
        {
            try {
                Process.Start(string.Format("{0}/lokinet.ini", config_path)); }
            catch
            {
                MessageBox.Show("No existing config found");
                BtnNewCfg_Click(sender, e);
            }

        }

        private void BtnNewCfg_Click(object sender, EventArgs e)
        {
            if (File.Exists(string.Format("{0}/lokinet.ini", config_path)))
            {
                DialogResult resp = MessageBox.Show("WARNING: This will overwrite your existing config file, Continue?", "Lokinet", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
                switch(resp)
                {
                    case DialogResult.Yes:
                        File.Delete(string.Format("{0}/lokinet.ini", config_path));
                        break;
                    case DialogResult.No:
                        return;
                }
            }
            
            string lokinetExeString;

            if (Program.platform == PlatformID.Win32NT)
                lokinetExeString = String.Format("{0}\\lokinet.exe", Directory.GetCurrentDirectory());
            else
                lokinetExeString = String.Format("{0}/lokinet", Directory.GetCurrentDirectory());
            Process p = new Process();
            p.StartInfo.FileName = lokinetExeString;
            p.StartInfo.Arguments = "-g";
            p.StartInfo.CreateNoWindow = true;
            p.StartInfo.UseShellExecute = false;
            p.EnableRaisingEvents = true;
            p.Exited += new EventHandler(msg);
            p.Start();
        }

        private void msg(object sender, EventArgs e)
        {
            MessageBox.Show(string.Format("Created new config file at {0}/lokinet.ini", config_path), "Success", MessageBoxButtons.OK, MessageBoxIcon.Asterisk);
        }
    }
}
