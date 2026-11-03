// Native plugin installer. No OBS configuration, service, browser, or network API.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;
using System.Threading;
using System.Windows.Forms;

[assembly: AssemblyTitle("VEIL OBS Filter Setup")]
[assembly: AssemblyProduct("VEIL")]
[assembly: AssemblyVersion("0.4.7.0")]
[assembly: AssemblyFileVersion("0.4.7.0")]

namespace TrueitemInstaller
{
    internal sealed class PluginInstaller
    {
        internal const string PluginName = "obs-trueitem-beauty";
        internal readonly string Target;
        internal readonly string Storage;
        private readonly bool isSelfTest;
        private string Backups { get { return Path.Combine(Storage, "backups"); } }
        private string Marker { get { return Path.Combine(Storage, "latest-backup.txt"); } }

        internal PluginInstaller(string target, string storage, bool selfTest)
        {
            Target = Path.GetFullPath(target);
            Storage = Path.GetFullPath(storage);
            isSelfTest = selfTest;
            if (String.Equals(Target, Storage, StringComparison.OrdinalIgnoreCase) ||
                IsUnder(Target, Storage) || IsUnder(Storage, Target))
                throw new IOException("설치 폴더와 백업 폴더는 서로 분리되어야 합니다.");
        }

        private static bool IsUnder(string child, string parent)
        {
            return Path.GetFullPath(child).StartsWith(Path.GetFullPath(parent).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar,
                StringComparison.OrdinalIgnoreCase);
        }

        private void CheckOBS()
        {
            if (isSelfTest) return;
            foreach (string name in new string[] { "obs64", "obs32" })
            {
                Process[] processes = Process.GetProcessesByName(name);
                bool running = processes.Length != 0;
                foreach (Process process in processes) process.Dispose();
                if (running) throw new IOException("OBS가 실행 중입니다. OBS를 완전히 종료한 뒤 다시 설치해 주세요. 방송이나 녹화를 강제로 종료하지 않습니다.");
            }
        }

        internal void CheckBeforeElevation() { CheckOBS(); }

        private static void RejectReparsePoints(string path)
        {
            DirectoryInfo directory = new DirectoryInfo(Path.GetFullPath(path));
            while (directory != null)
            {
                if (directory.Exists && (directory.Attributes & FileAttributes.ReparsePoint) != 0)
                    throw new IOException("연결된 폴더에는 설치하지 않습니다: " + directory.FullName);
                directory = directory.Parent;
            }
        }

        private static void ValidatePlugin(string directory)
        {
            foreach (string file in new string[] {
                "bin/64bit/obs-trueitem-beauty.dll", "bin/64bit/onnxruntime.dll", "bin/64bit/onnxruntime_providers_shared.dll",
                "data/beauty.effect", "data/engines/video/bin/64bit/veil-video-matting.dll", "data/engines/finish/veil-background-finish.dll",
                "data/engines/video/bin/64bit/veil-matting-ort.dll", "data/engines/video/bin/64bit/DirectML.dll",
                "data/engines/video/data/models/rvm.onnx", "data/engines/video/data/models/bmv2.onnx",
                "data/engines/lite/bin/64bit/live-backgroundremoval-lite.dll",
                "data/engines/lite/data/effects/main.effect", "data/engines/advanced/obs-advanced-masks.dll",
                "data/engines/advanced/data/shaders/chroma-key.effect", "data/engines/advanced/data/shaders/feather-mask.effect",
                "data/engines/advanced/data/shaders/jump-flood.effect", "data/engines/advanced/data/shaders/super-key.effect", "data/models/face_detector.onnx",
                "data/models/face_landmarks.onnx", "data/user-preset.json",
                "data/licenses/TRUEITEM-LICENSE.txt", "data/licenses/THIRD-PARTY-NOTICES.md" })
            {
                string path = Path.Combine(directory, file.Replace('/', Path.DirectorySeparatorChar));
                if (!File.Exists(path) || new FileInfo(path).Length == 0)
                    throw new IOException("설치 파일이 누락되었습니다: " + file);
            }
        }

        private static void ValidateOwnedTarget(string target)
        {
            if (!Directory.Exists(target)) return;
            string dll = Path.Combine(target, "bin", "64bit", PluginName + ".dll");
            if (!File.Exists(dll))
                throw new IOException("대상 폴더에 다른 파일이 있습니다. 안전을 위해 덮어쓰지 않았습니다: " + target);
        }

        private static void ExtractPayload(string directory)
        {
            Assembly assembly = Assembly.GetExecutingAssembly();
            using (Stream input = assembly.GetManifestResourceStream("Trueitem.Payload.zip"))
            using (Stream digest = assembly.GetManifestResourceStream("Trueitem.Payload.sha256"))
            {
                if (input == null || digest == null) throw new IOException("내장 설치 파일이 없습니다.");
                string expected;
                using (StreamReader reader = new StreamReader(digest, Encoding.ASCII)) expected = reader.ReadToEnd().Trim();
                string actual;
                using (SHA256 sha = SHA256.Create()) actual = BitConverter.ToString(sha.ComputeHash(input)).Replace("-", "").ToLowerInvariant();
                if (!String.Equals(expected, actual, StringComparison.Ordinal)) throw new IOException("설치 파일 무결성 검사에 실패했습니다.");
                input.Position = 0;
                using (ZipArchive archive = new ZipArchive(input, ZipArchiveMode.Read))
                {
                    foreach (ZipArchiveEntry entry in archive.Entries)
                    {
                        string name = entry.FullName.Replace('/', Path.DirectorySeparatorChar);
                        if (Path.IsPathRooted(name) || name.IndexOf(':') >= 0)
                            throw new IOException("잘못된 설치 파일 경로입니다.");
                        string path = Path.GetFullPath(Path.Combine(directory, name));
                        if (!IsUnder(path, directory)) throw new IOException("설치 폴더 밖으로 나가는 파일이 있습니다.");
                        if (entry.Name.Length == 0) { Directory.CreateDirectory(path); continue; }
                        Directory.CreateDirectory(Path.GetDirectoryName(path));
                        using (Stream source = entry.Open())
                        using (FileStream output = new FileStream(path, FileMode.CreateNew, FileAccess.Write)) source.CopyTo(output);
                    }
                }
            }
        }

        private string NewBackup(string prefix)
        {
            return Path.Combine(Backups, prefix + DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N"));
        }

        internal void Install(bool simulateFailure)
        {
            RunLocked(delegate { InstallCore(simulateFailure); });
        }

        private static void RunLocked(Action action)
        {
            using (Mutex mutex = new Mutex(false, "Local\\TRUEITEM-OBS-Beauty-Installer"))
            {
                bool owned = false;
                try
                {
                    try { owned = mutex.WaitOne(0, false); }
                    catch (AbandonedMutexException) { owned = true; }
                if (!owned) throw new IOException("다른 VEIL 설치 작업이 실행 중입니다.");
                    action();
                }
                finally { if (owned) mutex.ReleaseMutex(); }
            }
        }

        private void InstallCore(bool simulateFailure)
        {
            CheckOBS();
            RejectReparsePoints(Target);
            RejectReparsePoints(Storage);
            ValidateOwnedTarget(Target);
            Directory.CreateDirectory(Storage);
            Directory.CreateDirectory(Backups);
            string stage = Path.Combine(Storage, "staging", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(stage);
            ExtractPayload(stage);
            ValidatePlugin(stage);
            File.WriteAllText(Path.Combine(stage, "TRUEITEM-INSTALLATION.txt"), "VEIL 0.4.7 native OBS beauty/video-matting plugin\r\n", Encoding.UTF8);
            // Recheck just before the directory swap. Never stop a running OBS.
            CheckOBS();
            Directory.CreateDirectory(Path.GetDirectoryName(Target));
            string backup = Directory.Exists(Target) ? NewBackup("previous-") : null;
            if (backup != null) Directory.Move(Target, backup);
            bool movedNew = false;
            try
            {
                if (simulateFailure) throw new IOException("SELFTEST simulated failure after backup");
                Directory.Move(stage, Target);
                movedNew = true;
                string nextMarker = Path.Combine(Storage, "marker-" + Guid.NewGuid().ToString("N") + ".tmp");
                File.WriteAllText(nextMarker, backup == null ? "ABSENT" : Path.GetFileName(backup), Encoding.ASCII);
                if (File.Exists(Marker)) File.Replace(nextMarker, Marker, null); else File.Move(nextMarker, Marker);
            }
            catch
            {
                if (movedNew && Directory.Exists(Target)) Directory.Move(Target, NewBackup("failed-new-"));
                if (backup != null && Directory.Exists(backup)) Directory.Move(backup, Target);
                throw;
            }
        }

        internal void Rollback()
        {
            RunLocked(RollbackCore);
        }

        private void RollbackCore()
        {
            CheckOBS();
            RejectReparsePoints(Target);
            RejectReparsePoints(Storage);
            if (!File.Exists(Marker)) throw new IOException("되돌릴 설치 기록이 없습니다.");
            ValidateOwnedTarget(Target);
            string name = File.ReadAllText(Marker, Encoding.ASCII).Trim();
            string backup = null;
            if (name != "ABSENT")
            {
                if (name != Path.GetFileName(name) || !name.StartsWith("previous-", StringComparison.Ordinal))
                    throw new IOException("잘못된 백업 기록입니다.");
                backup = Path.GetFullPath(Path.Combine(Backups, name));
                if (!IsUnder(backup, Backups) || !Directory.Exists(backup)) throw new IOException("백업 폴더를 찾을 수 없습니다.");
                ValidateOwnedTarget(backup);
            }
            string retired = Directory.Exists(Target) ? NewBackup("rolled-back-") : null;
            if (retired != null) Directory.Move(Target, retired);
            bool restored = false;
            try
            {
                if (backup != null) { Directory.Move(backup, Target); restored = true; }
                File.Move(Marker, Path.Combine(Storage, "applied-" + Guid.NewGuid().ToString("N") + ".txt"));
            }
            catch
            {
                if (restored) Directory.Move(Target, backup);
                if (retired != null) Directory.Move(retired, Target);
                throw;
            }
        }

        private static string Snapshot(string path)
        {
            List<string> hashes = new List<string>();
            foreach (string file in Directory.GetFiles(path, "*", SearchOption.AllDirectories))
                using (SHA256 hash = SHA256.Create())
                using (FileStream stream = File.OpenRead(file))
                    hashes.Add(file.Substring(path.Length) + ":" + BitConverter.ToString(hash.ComputeHash(stream)));
            hashes.Sort(StringComparer.Ordinal);
            return String.Join("\n", hashes.ToArray());
        }

        internal void CheckWriteAccess()
        {
            RejectReparsePoints(Target);
            RejectReparsePoints(Storage);
            foreach (string directory in new string[] { Path.GetDirectoryName(Target), Storage })
            {
                Directory.CreateDirectory(directory);
                string probe = Path.Combine(directory, ".trueitem-write-probe-" + Guid.NewGuid().ToString("N"));
                using (FileStream file = new FileStream(probe, FileMode.CreateNew, FileAccess.Write, FileShare.None, 1, FileOptions.DeleteOnClose)) file.WriteByte(1);
            }
        }

        internal static int SelfTest(string root)
        {
            string area = Path.Combine(Path.GetFullPath(root), "installer-selftest-" + Guid.NewGuid().ToString("N"));
            string plugins = Path.Combine(area, "plugins");
            string target = Path.Combine(plugins, PluginName);
            PluginInstaller installer = new PluginInstaller(target, Path.Combine(area, "manager"), true);
            Directory.CreateDirectory(Path.Combine(plugins, "unrelated-plugin"));
            string canary = Path.Combine(plugins, "unrelated-plugin", "keep.txt");
            File.WriteAllText(canary, "KEEP");
            string userConfig = Path.Combine(area, "obs-user-config", "veil-video-matting", "backgrounds");
            Directory.CreateDirectory(userConfig);
            byte[] generatedPlate = new byte[16 + 64 * 48 * 4];
            Array.Copy(Encoding.ASCII.GetBytes("VEILBG01"), generatedPlate, 8);
            Array.Copy(BitConverter.GetBytes(64), 0, generatedPlate, 8, 4);
            Array.Copy(BitConverter.GetBytes(48), 0, generatedPlate, 12, 4);
            for (int i = 16; i < generatedPlate.Length; ++i) generatedPlate[i] = (byte)(i % 251);
            File.WriteAllBytes(Path.Combine(userConfig, "generated-reference.veilbg"), generatedPlate);
            string referenceSnapshot = Snapshot(userConfig);
            Action assertReference = delegate { if (Snapshot(userConfig) != referenceSnapshot) throw new Exception("Registered background changed during installation/update/rollback"); };
            installer.Install(false);
            assertReference();
            ValidatePlugin(target);
            string original = Snapshot(target);
            installer.Rollback();
            assertReference();
            if (Directory.Exists(target)) throw new Exception("First-install rollback did not restore absence");
            installer.Install(false);
            File.WriteAllText(Path.Combine(target, "existing-user-note.txt"), "preserve in backup");
            string previous = Snapshot(target);
            bool failed = false;
            try { installer.Install(true); } catch (IOException) { failed = true; }
            if (!failed || Snapshot(target) != previous) throw new Exception("Failed update did not restore original bytes");
            assertReference();
            installer.Install(false);
            assertReference();
            if (Snapshot(target) != original) throw new Exception("Installed payload differs from original");
            installer.Rollback();
            assertReference();
            if (Snapshot(target) != previous) throw new Exception("Update rollback did not restore original bytes");
            if (File.ReadAllText(canary) != "KEEP") throw new Exception("Unrelated plugin changed");
            string guardEvidence = "SKIP OBS-running guard: OBS not running during test.";
            Process[] runningOBS = Process.GetProcessesByName("obs64");
            bool obsWasRunning = runningOBS.Length > 0;
            foreach (Process process in runningOBS) process.Dispose();
            if (obsWasRunning)
            {
                string guardedTarget = Path.Combine(area, "guard-test", PluginName);
                PluginInstaller guarded = new PluginInstaller(guardedTarget, Path.Combine(area, "guard-storage"), false);
                bool blocked = false;
                try { guarded.Install(false); }
                catch (IOException error) { blocked = error.Message.StartsWith("OBS가 실행 중입니다.", StringComparison.Ordinal); }
                if (!blocked || Directory.Exists(guardedTarget) || Directory.Exists(guarded.Storage))
                    throw new Exception("OBS-running guard did not refuse before touching the custom destination");
                guardEvidence = "PASS OBS-running guard: refused before creating custom destination/storage; running OBS untouched.";
            }
            File.WriteAllText(Path.Combine(area, "SELFTEST-PASS.txt"),
                "PASS: fresh install, first-install rollback, failed-update recovery, update, byte-exact rollback, unrelated-plugin preservation, registered-background preservation outside plugin target.\r\n" + guardEvidence + "\r\n", Encoding.UTF8);
            return 0;
        }
    }

    internal sealed class SetupForm : Form
    {
        private readonly Label statusTitle;
        private readonly Label statusDetail;
        private readonly Label statusSymbol;
        private readonly Button install;
        private readonly Button rollback;
        private readonly Button close;
        private readonly ProgressBar progress;
        private readonly LinkLabel detailsToggle;
        private readonly TextBox details;
        private readonly ToolTip hints;
        private readonly PluginInstaller installer;
        private readonly string initialAction;
        private readonly string installLabel;
        private readonly Color ink;
        private readonly Color muted;
        private readonly Color accent;
        private readonly Color accentInk;
        private bool working;
        private bool closeRequested;
        private bool complete;
        private string errorDetail;

        internal SetupForm(string action)
        {
            initialAction = action;
            string programData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
            installer = new PluginInstaller(Path.Combine(programData, "obs-studio", "plugins", PluginInstaller.PluginName),
                Path.Combine(programData, "TRUEITEM-OBS-Beauty"), false);
            bool highContrast = SystemInformation.HighContrast;
            ink = highContrast ? SystemColors.WindowText : Color.FromArgb(30, 34, 48);
            muted = highContrast ? SystemColors.WindowText : Color.FromArgb(83, 90, 109);
            accent = highContrast ? SystemColors.Highlight : Color.FromArgb(64, 59, 104);
            accentInk = highContrast ? SystemColors.HighlightText : Color.White;
            BackColor = highContrast ? SystemColors.Window : Color.FromArgb(247, 248, 251);
            ForeColor = ink;
            Text = "VEIL 설치";
            ClientSize = new Size(680, 480);
            MinimumSize = new Size(696, 519);
            FormBorderStyle = FormBorderStyle.Sizable;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            Font = new Font("Malgun Gothic", 10F);
            AutoScaleDimensions = new SizeF(96F, 96F);
            AutoScaleMode = AutoScaleMode.Dpi;
            AutoScroll = true;
            Padding = new Padding(32, 28, 32, 28);

            TableLayoutPanel layout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 7, Margin = Padding.Empty, Padding = Padding.Empty };
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 26));
            layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 46));
            Controls.Add(layout);

            TableLayoutPanel header = new TableLayoutPanel { AutoSize = true, Dock = DockStyle.Top, ColumnCount = 1, Margin = Padding.Empty, Padding = Padding.Empty };
            Label brand = new Label { Text = "VEIL", Font = new Font(Font.FontFamily, 32F, FontStyle.Bold), AutoSize = true, Margin = new Padding(0, 0, 0, 4), ForeColor = ink };
            Label summary = new Label { Text = "OBS용 얼굴 보정 · 배경 제거", AutoSize = true, Margin = new Padding(2, 0, 0, 8), ForeColor = ink };
            Label version = new Label { Text = "버전 0.4.7", Font = new Font(Font.FontFamily, 9F), AutoSize = true, ForeColor = muted, Margin = new Padding(2, 0, 0, 0) };
            header.Controls.Add(brand); header.Controls.Add(summary); header.Controls.Add(version);
            layout.Controls.Add(header, 0, 0);

            TableLayoutPanel card = new TableLayoutPanel { AutoSize = true, Dock = DockStyle.Top, ColumnCount = 2, RowCount = 2, Padding = new Padding(18, 18, 18, 18), Margin = Padding.Empty, BackColor = highContrast ? SystemColors.Window : Color.White };
            card.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 36));
            card.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            card.Paint += delegate(object sender, PaintEventArgs e) { using (Pen border = new Pen(highContrast ? SystemColors.WindowText : Color.FromArgb(221, 224, 233))) e.Graphics.DrawRectangle(border, 0, 0, card.ClientSize.Width - 1, card.ClientSize.Height - 1); };
            statusSymbol = new Label { Text = "•", Font = new Font(Font.FontFamily, 20F, FontStyle.Bold), AutoSize = true, ForeColor = accent, Margin = new Padding(0, 0, 6, 0), AccessibleName = "준비" };
            statusTitle = new Label { Text = "설치 준비 완료", Font = new Font(Font.FontFamily, 13F, FontStyle.Bold), AutoSize = true, Dock = DockStyle.Fill, ForeColor = ink, Margin = new Padding(0, 0, 0, 8) };
            statusDetail = new Label { Text = "OBS를 종료한 뒤 시작하세요.", AutoSize = true, Dock = DockStyle.Fill, ForeColor = muted, Margin = Padding.Empty, MaximumSize = new Size(0, 72) };
            card.Controls.Add(statusSymbol, 0, 0); card.SetRowSpan(statusSymbol, 2);
            card.Controls.Add(statusTitle, 1, 0); card.Controls.Add(statusDetail, 1, 1);
            layout.Controls.Add(card, 0, 2);

            progress = new ProgressBar { Dock = DockStyle.Top, Height = 5, Style = ProgressBarStyle.Marquee, MarqueeAnimationSpeed = 25, Visible = false, Margin = new Padding(0, 10, 0, 0), AccessibleName = "설치 진행 중", TabStop = false };
            layout.Controls.Add(progress, 0, 3);

            TableLayoutPanel detailArea = new TableLayoutPanel { AutoSize = true, Dock = DockStyle.Top, ColumnCount = 1, Margin = new Padding(0, 12, 0, 0), Padding = Padding.Empty };
            detailsToggle = new LinkLabel { Text = "설치 위치 보기", AutoSize = true, LinkColor = accent, ActiveLinkColor = accent, VisitedLinkColor = accent, Margin = Padding.Empty, TabIndex = 2, AccessibleName = "설치 위치 펼치기" };
            details = new TextBox { ReadOnly = true, Multiline = true, BorderStyle = BorderStyle.None, Dock = DockStyle.Top, Height = 66, WordWrap = true, ScrollBars = ScrollBars.Vertical, Visible = false, BackColor = BackColor, ForeColor = muted, Font = new Font(Font.FontFamily, 9F), Margin = new Padding(0, 10, 0, 0), TabIndex = 3, AccessibleName = "설치 위치" };
            details.Text = installer.Target;
            detailsToggle.LinkClicked += delegate { ToggleDetails(); };
            detailArea.Controls.Add(detailsToggle); detailArea.Controls.Add(details);
            layout.Controls.Add(detailArea, 0, 4);

            TableLayoutPanel actions = new TableLayoutPanel { AutoSize = false, Height = 46, Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Margin = Padding.Empty, Padding = Padding.Empty };
            actions.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            actions.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            actions.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            actions.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            installLabel = File.Exists(Path.Combine(installer.Target, "bin", "64bit", PluginInstaller.PluginName + ".dll")) ? "VEIL 업데이트" : "VEIL 설치";
            install = new Button { Text = installLabel, Dock = DockStyle.Fill, MinimumSize = new Size(206, 46), AutoSize = true, BackColor = accent, ForeColor = accentInk, FlatStyle = FlatStyle.Flat, Font = new Font(Font.FontFamily, 10F, FontStyle.Bold), Margin = new Padding(0, 0, 12, 0), TabIndex = 0, AccessibleName = installLabel, UseVisualStyleBackColor = false };
            install.FlatAppearance.BorderSize = highContrast ? 1 : 0;
            rollback = new Button { Text = "설치 되돌리기", AutoSize = true, Dock = DockStyle.Fill, MinimumSize = new Size(144, 46), Margin = new Padding(0, 0, 12, 0), TabIndex = 1, AccessibleName = "이전 설치로 되돌리기", UseVisualStyleBackColor = true };
            close = new Button { Text = "닫기", AutoSize = true, Dock = DockStyle.Fill, MinimumSize = new Size(88, 46), Margin = Padding.Empty, TabIndex = 4, DialogResult = DialogResult.Cancel, AccessibleName = "설치창 닫기", UseVisualStyleBackColor = true };
            actions.Controls.Add(install, 0, 0); actions.Controls.Add(rollback, 1, 0); actions.Controls.Add(close, 2, 0);
            layout.Controls.Add(actions, 0, 6);
            AcceptButton = install; CancelButton = close;
            hints = new ToolTip { AutoPopDelay = 12000, InitialDelay = 400, ReshowDelay = 100 };
            hints.SetToolTip(install, "Enter 키로 시작합니다. 필요한 경우 Windows 관리자 권한을 요청합니다.");
            hints.SetToolTip(rollback, "업데이트 전 상태로 되돌립니다. 처음 설치했다면 VEIL을 제거합니다.");
            hints.SetToolTip(close, "Esc 키로 닫습니다. 파일을 교체하는 중에는 완료 후 닫습니다.");
            UpdateRollbackAvailability();
            install.Click += delegate { if (complete) Close(); else RunAction("--install"); };
            rollback.Click += delegate { RunAction("--rollback"); };
            close.Click += delegate { RequestClose(); };
            FormClosing += delegate(object sender, FormClosingEventArgs e) { if (working) { e.Cancel = true; RequestClose(); } };
            Shown += delegate { install.Focus(); if (!String.IsNullOrEmpty(initialAction)) BeginInvoke(new Action(delegate { RunAction(initialAction); })); };
        }

        private void UpdateRollbackAvailability()
        {
            rollback.Enabled = !working && File.Exists(Path.Combine(installer.Storage, "latest-backup.txt"));
            if (!rollback.Enabled) hints.SetToolTip(rollback, working ? "작업이 끝난 뒤 사용할 수 있습니다." : "되돌릴 이전 설치 기록이 없습니다.");
            else hints.SetToolTip(rollback, "업데이트 전 상태로 되돌립니다. 처음 설치했다면 VEIL을 제거합니다.");
        }

        private void ToggleDetails()
        {
            details.Visible = !details.Visible;
            detailsToggle.Text = details.Visible ? "접기" : String.IsNullOrEmpty(errorDetail) ? "설치 위치 보기" : "오류 자세히 보기";
            detailsToggle.AccessibleName = details.Visible ? "자세한 내용 접기" : String.IsNullOrEmpty(errorDetail) ? "설치 위치 펼치기" : "오류 자세히 펼치기";
        }

        private void RequestClose()
        {
            if (!working) { Close(); return; }
            closeRequested = true;
            close.Text = "닫기 예약됨";
            statusDetail.Text = "작업이 끝나면 이 창을 닫습니다.";
        }

        private void SetStatus(string symbol, string title, string message)
        {
            statusSymbol.Text = symbol; statusSymbol.AccessibleName = title;
            statusTitle.Text = title; statusTitle.AccessibleName = title;
            statusDetail.Text = message;
            AccessibilityNotifyClients(AccessibleEvents.SystemAlert, -1);
        }

        private void ShowError(Exception error, string action)
        {
            complete = false;
            install.Text = installLabel;
            install.AccessibleName = installLabel;
            close.Visible = true;
            errorDetail = error.Message;
            details.Text = errorDetail + "\r\n\r\n설치 위치\r\n" + installer.Target;
            details.AccessibleName = "오류 내용과 설치 위치";
            details.Visible = false;
            detailsToggle.Text = "오류 자세히 보기";
            detailsToggle.AccessibleName = "오류 자세히 펼치기";
            if (error.Message.StartsWith("OBS가 실행 중입니다.", StringComparison.Ordinal))
                SetStatus("!", "OBS를 종료해 주세요", "OBS를 완전히 종료한 뒤 다시 눌러 주세요.");
            else if (error is System.ComponentModel.Win32Exception && ((System.ComponentModel.Win32Exception)error).NativeErrorCode == 1223)
                SetStatus("•", "설치를 취소했습니다", "준비되면 다시 시작할 수 있습니다.");
            else if (error is UnauthorizedAccessException)
                SetStatus("!", "설치 권한이 필요합니다", "관리자 권한으로 다시 실행해 주세요.");
            else SetStatus("!", action == "--rollback" ? "되돌리지 못했습니다" : "설치를 완료하지 못했습니다", "오류 내용을 확인한 뒤 다시 시도해 주세요.");
            install.Focus();
        }

        private void RunAction(string action)
        {
            if (working) return;
            try
            {
                installer.CheckBeforeElevation();
                using (WindowsIdentity identity = WindowsIdentity.GetCurrent())
                {
                    if (!new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator))
                    {
                        Process.Start(new ProcessStartInfo(Assembly.GetExecutingAssembly().Location, action) { UseShellExecute = true, Verb = "runas" });
                        Close();
                        return;
                    }
                }
                working = true; complete = false; close.Visible = true;
                install.Enabled = false;
                UpdateRollbackAvailability();
                errorDetail = null;
                details.Visible = false; details.Text = installer.Target; details.AccessibleName = "설치 위치";
                detailsToggle.Text = "설치 위치 보기";
                detailsToggle.AccessibleName = "설치 위치 펼치기";
                progress.Visible = true;
                progress.AccessibleName = action == "--rollback" ? "이전 설치 복원 중" : "설치 진행 중";
                install.Text = action == "--rollback" ? "되돌리는 중…" : "설치하는 중…";
                SetStatus("•", action == "--rollback" ? "설치를 되돌리는 중" : "VEIL을 설치하는 중", "잠시만 기다려 주세요.");
                // Keep the window responsive. A close request waits for the same
                // atomic Install/Rollback operation to finish; files are never
                // abandoned half-way through a directory replacement.
                ThreadPool.QueueUserWorkItem(delegate
                {
                    Exception failure = null;
                    try { if (action == "--rollback") installer.Rollback(); else installer.Install(false); }
                    catch (Exception error) { failure = error; }
                    BeginInvoke(new Action(delegate
                    {
                        working = false; progress.Visible = false;
                        install.Enabled = true; UpdateRollbackAvailability();
                        if (failure != null) ShowError(failure, action);
                        else
                        {
                            complete = true;
                            SetStatus("✓", action == "--rollback" ? "설치를 되돌렸습니다" : "VEIL이 준비되었습니다", "OBS를 실행하면 적용됩니다.");
                            install.Text = "완료"; install.AccessibleName = "설치 완료 후 닫기";
                            close.Visible = false;
                            install.Focus();
                        }
                        if (closeRequested) Close();
                    }));
                });
            }
            catch (Exception error)
            {
                working = false; progress.Visible = false; install.Enabled = true; UpdateRollbackAvailability();
                ShowError(error, action);
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && hints != null) hints.Dispose();
            base.Dispose(disposing);
        }
    }

    internal static class Program
    {
        [STAThread]
        private static int Main(string[] args)
        {
            if (args.Length == 2 && args[0] == "--self-test")
            {
                try { return PluginInstaller.SelfTest(args[1]); }
                catch (Exception error)
                {
                    Directory.CreateDirectory(args[1]);
                    File.WriteAllText(Path.Combine(args[1], "SELFTEST-ERROR.txt"), error.ToString());
                    return 1;
                }
            }
            if (args.Length == 1 && (args[0] == "--install-cli" || args[0] == "--rollback-cli" || args[0] == "--check-write"))
            {
                string report = Assembly.GetExecutingAssembly().Location + ".last-operation.txt";
                try
                {
                    string programData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
                    PluginInstaller installer = new PluginInstaller(Path.Combine(programData, "obs-studio", "plugins", PluginInstaller.PluginName),
                        Path.Combine(programData, "TRUEITEM-OBS-Beauty"), false);
                    if (args[0] == "--install-cli") installer.Install(false);
                    else if (args[0] == "--rollback-cli") installer.Rollback();
                    else installer.CheckWriteAccess();
                    File.WriteAllText(report, "PASS " + args[0] + "\r\n" + installer.Target + "\r\n", Encoding.UTF8);
                    return 0;
                }
                catch (Exception error)
                {
                    try { File.WriteAllText(report, "FAIL " + args[0] + "\r\n" + error.ToString(), Encoding.UTF8); } catch { }
                    return error is UnauthorizedAccessException ? 5 : 1;
                }
            }
            if (args.Length > 1 || (args.Length == 1 && args[0] != "--install" && args[0] != "--rollback")) return 2;
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new SetupForm(args.Length == 1 ? args[0] : null));
            return 0;
        }
    }
}

