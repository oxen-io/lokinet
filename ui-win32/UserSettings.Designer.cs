namespace network.loki.lokinet.win32.ui
{
    partial class UserSettingsForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.btnOK = new System.Windows.Forms.Button();
            this.btnBoot = new System.Windows.Forms.Button();
            this.btnDumpLog = new System.Windows.Forms.Button();
            this.btnVSettings = new System.Windows.Forms.Button();
            this.btnEditCfg = new System.Windows.Forms.Button();
            this.btnNewCfg = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // btnOK
            // 
            this.btnOK.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.btnOK.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.btnOK.Location = new System.Drawing.Point(109, 167);
            this.btnOK.Name = "btnOK";
            this.btnOK.Size = new System.Drawing.Size(75, 23);
            this.btnOK.TabIndex = 0;
            this.btnOK.Text = "Close";
            this.btnOK.UseVisualStyleBackColor = true;
            this.btnOK.Click += new System.EventHandler(this.btnOK_Click);
            // 
            // btnBoot
            // 
            this.btnBoot.Location = new System.Drawing.Point(13, 13);
            this.btnBoot.Name = "btnBoot";
            this.btnBoot.Size = new System.Drawing.Size(270, 23);
            this.btnBoot.TabIndex = 1;
            this.btnBoot.Text = "Bootstrap Client from Web...";
            this.btnBoot.UseVisualStyleBackColor = true;
            this.btnBoot.Click += new System.EventHandler(this.btnBoot_Click);
            // 
            // btnDumpLog
            // 
            this.btnDumpLog.Location = new System.Drawing.Point(13, 43);
            this.btnDumpLog.Name = "btnDumpLog";
            this.btnDumpLog.Size = new System.Drawing.Size(270, 23);
            this.btnDumpLog.TabIndex = 2;
            this.btnDumpLog.Text = "Save Log...";
            this.btnDumpLog.UseVisualStyleBackColor = true;
            this.btnDumpLog.Click += new System.EventHandler(this.btnDumpLog_Click);
            // 
            // btnVSettings
            // 
            this.btnVSettings.Location = new System.Drawing.Point(13, 73);
            this.btnVSettings.Name = "btnVSettings";
            this.btnVSettings.Size = new System.Drawing.Size(270, 23);
            this.btnVSettings.TabIndex = 3;
            this.btnVSettings.Text = "Display Settings...";
            this.btnVSettings.UseVisualStyleBackColor = true;
            this.btnVSettings.Click += new System.EventHandler(this.btnVSettings_Click);
            // 
            // btnEditCfg
            // 
            this.btnEditCfg.Location = new System.Drawing.Point(13, 102);
            this.btnEditCfg.Name = "btnEditCfg";
            this.btnEditCfg.Size = new System.Drawing.Size(270, 23);
            this.btnEditCfg.TabIndex = 4;
            this.btnEditCfg.Text = "Edit Configuration File...";
            this.btnEditCfg.UseVisualStyleBackColor = true;
            this.btnEditCfg.Click += new System.EventHandler(this.BtnEditCfg_Click);
            // 
            // btnNewCfg
            // 
            this.btnNewCfg.Location = new System.Drawing.Point(12, 131);
            this.btnNewCfg.Name = "btnNewCfg";
            this.btnNewCfg.Size = new System.Drawing.Size(270, 23);
            this.btnNewCfg.TabIndex = 5;
            this.btnNewCfg.Text = "New Configuration File...";
            this.btnNewCfg.UseVisualStyleBackColor = true;
            this.btnNewCfg.Click += new System.EventHandler(this.BtnNewCfg_Click);
            // 
            // UserSettingsForm
            // 
            this.AcceptButton = this.btnOK;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this.btnOK;
            this.ClientSize = new System.Drawing.Size(295, 202);
            this.ControlBox = false;
            this.Controls.Add(this.btnNewCfg);
            this.Controls.Add(this.btnEditCfg);
            this.Controls.Add(this.btnVSettings);
            this.Controls.Add(this.btnDumpLog);
            this.Controls.Add(this.btnBoot);
            this.Controls.Add(this.btnOK);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "UserSettingsForm";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Settings";
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button btnOK;
        private System.Windows.Forms.Button btnBoot;
        private System.Windows.Forms.Button btnDumpLog;
        private System.Windows.Forms.Button btnVSettings;
        private System.Windows.Forms.Button btnEditCfg;
        private System.Windows.Forms.Button btnNewCfg;
    }
}