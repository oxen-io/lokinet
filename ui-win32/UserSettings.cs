using System;
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
    }
}
