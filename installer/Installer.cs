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
[assembly: AssemblyVersion("0.4.5.0")]
[assembly: AssemblyFileVersion("0.4.5.0")]

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
            File.WriteAllText(Path.Combine(stage, "TRUEITEM-INSTALLATION.txt"), "VEIL 0.4.5 native OBS beauty/video-matting plugin\r\n", Encoding.UTF8);
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
        private readonly Label status;
        private readonly Button install;
        private readonly Button rollback;
        private readonly PluginInstaller installer;
        private readonly string initialAction;

        internal SetupForm(string action)
        {
            initialAction = action;
            string programData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
            installer = new PluginInstaller(Path.Combine(programData, "obs-studio", "plugins", PluginInstaller.PluginName),
                Path.Combine(programData, "TRUEITEM-OBS-Beauty"), false);
            Text = "VEIL · OBS 필터 설치";
            ClientSize = new Size(660, 390);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            Font = new Font("Malgun Gothic", 10);
            AutoScaleMode = AutoScaleMode.Font;
            BackColor = Color.FromArgb(248, 248, 250);
            Label eyebrow = new Label { Text = "LIVE BEAUTY / OBS PLUGIN", ForeColor = Color.FromArgb(75, 80, 110), AutoSize = true, Location = new Point(28, 24) };
            Label title = new Label { Text = "VEIL", Font = new Font(Font.FontFamily, 27, FontStyle.Bold), AutoSize = true, Location = new Point(24, 46) };
            Label detail = new Label { Text = "자연스러운 얼굴 보정과 AI 배경 제거·크로마키.\r\nOBS를 완전히 종료한 뒤 설치해 주세요.\r\n저장한 방송 프리셋과 배경 엔진이 함께 설치됩니다.", Location = new Point(28, 110), Size = new Size(600, 83) };
            Label path = new Label { Text = "설치 위치\r\n" + installer.Target, Location = new Point(28, 203), Size = new Size(600, 50), ForeColor = Color.FromArgb(78, 78, 90) };
            status = new Label { Text = "설치를 시작하면 Windows 관리자 권한 확인이 표시될 수 있습니다.", Location = new Point(28, 266), Size = new Size(600, 48) };
            install = new Button { Text = "VEIL 설치 / 업데이트", Location = new Point(28, 326), Size = new Size(215, 40), BackColor = Color.FromArgb(59, 65, 112), ForeColor = Color.White, FlatStyle = FlatStyle.Flat };
            install.FlatAppearance.BorderSize = 0;
            rollback = new Button { Text = "이전 설치로 되돌리기", Location = new Point(253, 326), Size = new Size(220, 40) };
            Button close = new Button { Text = "닫기", Location = new Point(513, 326), Size = new Size(115, 40), DialogResult = DialogResult.Cancel };
            CancelButton = close;
            install.Click += delegate { RunAction("--install"); };
            rollback.Click += delegate { RunAction("--rollback"); };
            close.Click += delegate { Close(); };
            Controls.AddRange(new Control[] { eyebrow, title, detail, path, status, install, rollback, close });
            Shown += delegate { if (!String.IsNullOrEmpty(initialAction)) BeginInvoke(new Action(delegate { RunAction(initialAction); })); };
        }

        private void RunAction(string action)
        {
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
                install.Enabled = rollback.Enabled = false;
                status.Text = "파일을 확인하고 있습니다…";
                status.Refresh();
                if (action == "--rollback") installer.Rollback(); else installer.Install(false);
                status.Text = action == "--rollback" ? "복원이 완료되었습니다. OBS를 다시 실행해 주세요." :
                    "설치했습니다. OBS → 캠 소스 → 필터에서\r\n'VEIL 필터 · 얼굴·피부·메이크업'을 추가하세요.";
            }
            catch (Exception error)
            {
                status.Text = "변경을 완료하지 못했습니다. 아래 내용을 확인해 주세요.";
                MessageBox.Show(this, error.Message, "VEIL 설치 안내", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            finally { install.Enabled = rollback.Enabled = true; }
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
