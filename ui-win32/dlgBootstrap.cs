using System;
using System.Net;
using System.Windows.Forms;

namespace network.loki.lokinet.win32.ui
{
    public partial class dlgBootstrap : Form
    {
        public dlgBootstrap()
        {
            InitializeComponent();
            if (Program.platform == PlatformID.Win32NT)
                default_path = Environment.ExpandEnvironmentVariables("%APPDATA%\\.lokinet\\bootstrap.signed");
            else
                default_path = Environment.ExpandEnvironmentVariables("%HOME%/.lokinet/bootstrap.signed");
            label2.Text = String.Format("This file is automatically saved as {0}.", default_path);
        }

        private WebClient wc;
        private string default_path;
        private void button1_Click(object sender, EventArgs e)
        {
            wc = new WebClient();
            // add something more unique, this is the IE 5.0 default string
            wc.Headers.Add("User-Agent","Mozilla/4.0 (compatible; MSIE 5.0; Windows NT 5.0)");
            try
            {
                wc.DownloadFile(uriBox.Text, default_path);
                MessageBox.Show("LokiNET node bootstrapped", "LokiNET", MessageBoxButtons.OK, MessageBoxIcon.Information);
                DialogResult = DialogResult.OK;
            }
            catch (WebException ex)
            {
                MessageBox.Show(string.Format("An error occured while downloading data. {0}, error {1}", ex.Message, ex.InnerException.Message), "Error", MessageBoxButtons.OK, MessageBoxIcon.Hand);
                DialogResult = DialogResult.Abort;
            }
            wc.Dispose();
            Close();
        }

        private void button1_Click_1(object sender, EventArgs e)
        {
            DialogResult = DialogResult.Cancel;
            Close();
        }
    }
}
