namespace lokivpn
{
    partial class UI_Main_Frame
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
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(UI_Main_Frame));
            this.StatusLabel = new System.Windows.Forms.Label();
            this.lokinetd_fd1 = new System.Windows.Forms.TextBox();
            this.NotificationTrayIcon = new System.Windows.Forms.NotifyIcon(this.components);
            this.btnHide = new System.Windows.Forms.Button();
            this.UIVersionLabel = new System.Windows.Forms.Label();
            this.btnConnect = new System.Windows.Forms.Button();
            this.btnDrop = new System.Windows.Forms.Button();
            this.btnConfigProfile = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // StatusLabel
            // 
            this.StatusLabel.AutoSize = true;
            this.StatusLabel.Location = new System.Drawing.Point(13, 13);
            this.StatusLabel.Name = "StatusLabel";
            this.StatusLabel.Size = new System.Drawing.Size(97, 13);
            this.StatusLabel.TabIndex = 0;
            this.StatusLabel.Text = "[connection status]";
            // 
            // lokinetd_fd1
            // 
            this.lokinetd_fd1.AcceptsReturn = true;
            this.lokinetd_fd1.AcceptsTab = true;
            this.lokinetd_fd1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.lokinetd_fd1.BackColor = System.Drawing.SystemColors.InfoText;
            this.lokinetd_fd1.Font = new System.Drawing.Font("Iosevka Term Light", 9.749999F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.lokinetd_fd1.ForeColor = System.Drawing.Color.Lime;
            this.lokinetd_fd1.Location = new System.Drawing.Point(12, 39);
            this.lokinetd_fd1.MaxLength = 0;
            this.lokinetd_fd1.Multiline = true;
            this.lokinetd_fd1.Name = "lokinetd_fd1";
            this.lokinetd_fd1.ReadOnly = true;
            this.lokinetd_fd1.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.lokinetd_fd1.Size = new System.Drawing.Size(776, 330);
            this.lokinetd_fd1.TabIndex = 1;
            this.lokinetd_fd1.Text = "[pipe lokinetd fd 1 here]";
            // 
            // NotificationTrayIcon
            // 
            this.NotificationTrayIcon.Icon = ((System.Drawing.Icon)(resources.GetObject("NotificationTrayIcon.Icon")));
            this.NotificationTrayIcon.Text = "LokiNET for Windows - [status]";
            this.NotificationTrayIcon.Visible = true;
            // 
            // btnHide
            // 
            this.btnHide.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.btnHide.Location = new System.Drawing.Point(713, 415);
            this.btnHide.Name = "btnHide";
            this.btnHide.Size = new System.Drawing.Size(75, 23);
            this.btnHide.TabIndex = 2;
            this.btnHide.Text = "Hide";
            this.btnHide.UseVisualStyleBackColor = true;
            // 
            // UIVersionLabel
            // 
            this.UIVersionLabel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.UIVersionLabel.AutoSize = true;
            this.UIVersionLabel.Location = new System.Drawing.Point(529, 388);
            this.UIVersionLabel.Name = "UIVersionLabel";
            this.UIVersionLabel.Size = new System.Drawing.Size(259, 13);
            this.UIVersionLabel.TabIndex = 3;
            this.UIVersionLabel.Text = "LokiNET for Windows UI [version]/LokiNET [vcs-rev]";
            // 
            // btnConnect
            // 
            this.btnConnect.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.btnConnect.Location = new System.Drawing.Point(13, 415);
            this.btnConnect.Name = "btnConnect";
            this.btnConnect.Size = new System.Drawing.Size(75, 23);
            this.btnConnect.TabIndex = 4;
            this.btnConnect.Text = "Connect";
            this.btnConnect.UseVisualStyleBackColor = true;
            // 
            // btnDrop
            // 
            this.btnDrop.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.btnDrop.Location = new System.Drawing.Point(95, 414);
            this.btnDrop.Name = "btnDrop";
            this.btnDrop.Size = new System.Drawing.Size(75, 23);
            this.btnDrop.TabIndex = 5;
            this.btnDrop.Text = "Disconnect";
            this.btnDrop.UseVisualStyleBackColor = true;
            // 
            // btnConfigProfile
            // 
            this.btnConfigProfile.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.btnConfigProfile.Location = new System.Drawing.Point(177, 413);
            this.btnConfigProfile.Name = "btnConfigProfile";
            this.btnConfigProfile.Size = new System.Drawing.Size(75, 23);
            this.btnConfigProfile.TabIndex = 6;
            this.btnConfigProfile.Text = "Settings...";
            this.btnConfigProfile.UseVisualStyleBackColor = true;
            this.btnConfigProfile.Click += new System.EventHandler(this.btnConfigProfile_Click);
            // 
            // UI_Main_Frame
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(800, 450);
            this.Controls.Add(this.btnConfigProfile);
            this.Controls.Add(this.btnDrop);
            this.Controls.Add(this.btnConnect);
            this.Controls.Add(this.UIVersionLabel);
            this.Controls.Add(this.btnHide);
            this.Controls.Add(this.lokinetd_fd1);
            this.Controls.Add(this.StatusLabel);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "UI_Main_Frame";
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "LokiNET for Windows";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label StatusLabel;
        private System.Windows.Forms.TextBox lokinetd_fd1;
        private System.Windows.Forms.NotifyIcon NotificationTrayIcon;
        private System.Windows.Forms.Button btnHide;
        private System.Windows.Forms.Label UIVersionLabel;
        private System.Windows.Forms.Button btnConnect;
        private System.Windows.Forms.Button btnDrop;
        private System.Windows.Forms.Button btnConfigProfile;
    }
}

