using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace network.loki.lokinet.win32.ui
{
    public partial class VisualSettings : Form
    {
        public VisualSettings()
        {
            InitializeComponent();
            ToggleAutoScroll.Checked = Properties.Settings.Default.autoScroll;
        }

        private void btnOK_Click(object sender, EventArgs e)
        {
            if (ToggleAutoScroll.Checked)
                Properties.Settings.Default.autoScroll = true;
            else
                Properties.Settings.Default.autoScroll = false;
            Close();
        }
    }
}
