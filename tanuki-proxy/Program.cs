﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.Threading;
using System.Collections.Concurrent;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;

namespace tanuki_proxy
{
    public class Program
    {
        public enum UpstreamState
        {
            Stopped,
            Thinking,
        }

        public static object upstreamLockObject = new object();
        public static UpstreamState upstreamState = UpstreamState.Stopped;
        public static int depth = 0;
        public const string bestmoveNone = "None";
        public static string[] bestmoveBestMoves = null;
        public static string[] bestmovePonders = null;
        public static string upstreamPosition = "";
        public static int numberOfReadyoks = 0;
        public static System.Random random = new System.Random();

        [DataContract]
        public struct Option
        {
            [DataMember]
            public string name;
            [DataMember]
            public string value;

            public Option(string name, string value)
            {
                this.name = name;
                this.value = value;
            }
        }

        [DataContract]
        public class EngineOption
        {
            [DataMember]
            public string engineName { get; set; }
            [DataMember]
            public string fileName { get; set; }
            [DataMember]
            public string arguments { get; set; }
            [DataMember]
            public string workingDirectory { get; set; }
            [DataMember]
            public Option[] optionOverrides { get; set; }

            public EngineOption(string engineName, string fileName, string arguments, string workingDirectory, Option[] optionOverrides)
            {
                this.engineName = engineName;
                this.fileName = fileName;
                this.arguments = arguments;
                this.workingDirectory = workingDirectory;
                this.optionOverrides = optionOverrides;
            }

            public EngineOption()
            {
                //empty constructor for serialization
            }
        }

        [DataContract]
        public class ProxySetting
        {
            [DataMember]
            public EngineOption[] engines { get; set; }
            [DataMember]
            public string logDirectory { get; set; }
        }

        class Engine
        {
            private Process process = new Process();
            private Option[] optionOverrides;
            private string downstreamPosition = "";
            private object downstreamLockObject = new object();
            private BlockingCollection<string> commandQueue = new BlockingCollection<string>();
            private Thread thread;
            public string name { get; }
            private int id;

            public Engine(string engineName, string fileName, string arguments, string workingDirectory, Option[] optionOverrides, int id)
            {
                this.name = engineName;
                this.process.StartInfo.FileName = fileName;
                this.process.StartInfo.Arguments = arguments;
                this.process.StartInfo.WorkingDirectory = workingDirectory;
                this.process.StartInfo.UseShellExecute = false;
                this.process.StartInfo.RedirectStandardInput = true;
                this.process.StartInfo.RedirectStandardOutput = true;
                this.process.StartInfo.RedirectStandardError = true;
                this.process.OutputDataReceived += HandleStdout;
                this.process.ErrorDataReceived += HandleStderr;
                this.optionOverrides = optionOverrides;
                this.id = id;
            }

            public Engine(EngineOption opt, int id) : this(opt.engineName, opt.fileName, opt.arguments, opt.workingDirectory, opt.optionOverrides, id)
            {
            }

            public void RunAsync()
            {
                thread = new Thread(ThreadRun);
                thread.Start();
            }

            private void ThreadRun()
            {
                process.Start();
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();

                while (!commandQueue.IsCompleted)
                {
                    string input = null;
                    try
                    {
                        input = commandQueue.Take();
                    }
                    catch (InvalidOperationException)
                    {
                        continue;
                    }

                    if (string.IsNullOrEmpty(input))
                    {
                        continue;
                    }

                    var command = Split(input);
                    if (command.Count == 0)
                    {
                        continue;
                    }

                    // 将棋所：USIプロトコルとは http://www.geocities.jp/shogidokoro/usi.html
                    if (command.Contains("setoption"))
                    {
                        HandleSetoption(command);
                    }
                    else
                    {
                        Debug.WriteLine("  P> engine={0} command={1}", name, Join(command));
                        WriteLineAndFlush(process.StandardInput, Join(command));
                    }
                }
            }

            public void Close()
            {
                commandQueue.CompleteAdding();
                thread.Join();
                process.Close();
            }

            /// <summary>
            /// エンジンにコマンドを書き込む。
            /// </summary>
            /// <param name="input"></param>
            public void Write(string input)
            {
                // コマンドをBlockingCollectionに入れる。
                // 入れられたコマンドは別のスレッドで処理される。
                commandQueue.Add(input);
            }

            private static void WriteLineAndFlush(TextWriter textWriter, string format, params object[] arg)
            {
                textWriter.WriteLine(format, arg);
                textWriter.Flush();
            }

            private static string Join(IEnumerable<string> command)
            {
                return string.Join(" ", command);
            }

            /// <summary>
            /// setoptionコマンドを処理する
            /// </summary>
            /// <param name="input">UIまたは上流proxyからのコマンド文字列</param>
            /// <returns>コマンドをこの関数で処理した場合はtrue、そうでない場合はfalse</returns>
            private void HandleSetoption(List<string> command)
            {
                // エンジンに対して値を設定する時に送ります。
                Debug.Assert(command.Count == 5);
                Debug.Assert(command[1] == "name");
                Debug.Assert(command[3] == "value");

                // オプションをオーバーライドする
                foreach (var optionOverride in optionOverrides)
                {
                    if (command[2] == optionOverride.name)
                    {
                        command[4] = optionOverride.value;
                    }
                }

                Debug.WriteLine("  P> engine={0} command={1}", name, Join(command));
                WriteLineAndFlush(process.StandardInput, Join(command));
            }

            /// <summary>
            /// 思考エンジンの標準出力を処理する
            /// </summary>
            /// <param name="sender">出力を送ってきた思考エンジンのプロセス</param>
            /// <param name="e">思考エンジンの出力</param>
            private void HandleStdout(object sender, DataReceivedEventArgs e)
            {
                if (string.IsNullOrEmpty(e.Data))
                {
                    return;
                }

                Debug.WriteLine("  <D engine={0} command={1}", name, e.Data);

                var command = Split(e.Data);

                if (command.Contains("readyok"))
                {
                    HandleReadyok(command);
                }
                else if (command.Contains("position"))
                {
                    HandlePosition(command);
                }
                else if (command.Contains("pv"))
                {
                    HandlePv(command);
                }
                else if (command.Contains("bestmove"))
                {
                    HandleBestmove(command);
                }
                else
                {
                    Debug.WriteLine("<P   engine={0} command={1}", name, Join(command));
                    WriteLineAndFlush(Console.Out, Join(command));
                }
            }

            /// <summary>
            /// 思考エンジンの標準エラー出力を処理する
            /// </summary>
            /// <param name="sender">出力を送ってきた思考エンジンのプロセス</param>
            /// <param name="e">思考エンジンの出力</param>
            private void HandleStderr(object sender, DataReceivedEventArgs e)
            {
                if (string.IsNullOrEmpty(e.Data))
                {
                    return;
                }
                Debug.WriteLine("  <d engine={0} command={1}", name, e.Data);
            }

            private int getNumberOfRunningEngines()
            {
                int numberOfRunningEngines = 0;
                foreach (var engine in engines)
                {
                    if (!engine.process.HasExited)
                    {
                        ++numberOfRunningEngines;
                    }
                }
                return numberOfRunningEngines;
            }

            private void HandleReadyok(List<string> command)
            {
                // goコマンドが受理されたのでpvの受信を開始する
                lock (upstreamLockObject)
                {
                    if (getNumberOfRunningEngines() == ++numberOfReadyoks)
                    {
                        Debug.WriteLine("<P   engine={0} command={1}", name, Join(command));
                        WriteLineAndFlush(Console.Out, Join(command));
                    }
                }
            }

            /// <summary>
            /// info string startedを受信し、goコマンドが受理されたときの処理を行う
            /// </summary>
            /// <param name="command">思考エンジンの出力文字列</param>
            /// <returns>コマンドをこの関数で処理した場合はtrue、そうでない場合はfalse</returns>
            private void HandlePosition(List<string> command)
            {
                // goコマンドが受理されたのでpvの受信を開始する
                lock (downstreamLockObject)
                {
                    int index = command.IndexOf("position");
                    downstreamPosition = Join(command.Skip(index));
                    Debug.WriteLine("downstreamPosition=" + downstreamPosition);
                }
            }

            /// <summary>
            /// pvを含むinfoコマンドを処理する
            /// </summary>
            /// <param name="command">思考エンジンの出力文字列</param>
            /// <returns>コマンドをこの関数で処理した場合はtrue、そうでない場合はfalse</returns>
            private void HandlePv(List<string> command)
            {
                lock (upstreamLockObject)
                {
                    // 上流停止中はpvを含む行を処理しない
                    if (upstreamState == UpstreamState.Stopped)
                    {
                        //Debug.WriteLine("     ## process={0} upstreamState == UpstreamState.Stopped", process);
                        return;
                    }

                    lock (downstreamLockObject)
                    {
                        // 思考中の局面が違う場合は処理しない
                        if (upstreamPosition != downstreamPosition)
                        {
                            Debug.WriteLine("     ## process={0} upstreamGoIndex != downstreamGoIndex", process);
                            return;
                        }
                    }

                    int depthIndex = command.IndexOf("depth");
                    int pvIndex = command.IndexOf("pv");

                    if (depthIndex == -1 || pvIndex == -1)
                    {
                        return;
                    }

                    int tempDepth = int.Parse(command[depthIndex + 1]);

                    Debug.Assert(pvIndex + 1 < command.Count);
                    bestmoveBestMoves[id] = command[pvIndex + 1];
                    if (pvIndex + 2 < command.Count)
                    {
                        bestmovePonders[id] = command[pvIndex + 2];
                    }

                    Debug.WriteLine("<P   engine={0} command={1}", name, Join(command));

                    // Fail-low/Fail-highした探索結果は表示しない
                    if (!command.Contains("lowerbound") && !command.Contains("upperbound") && depth < tempDepth)
                    {
                        // voteの表示
                        Dictionary<string, int> bestmoveToCount = new Dictionary<string, int>();
                        for (int i = 0; i < bestmoveBestMoves.Length; ++i)
                        {
                            if (bestmoveBestMoves[i] == null)
                            {
                                continue;
                            }
                            if (!bestmoveToCount.ContainsKey(bestmoveBestMoves[i]))
                            {
                                bestmoveToCount.Add(bestmoveBestMoves[i], 0);
                            }
                            ++bestmoveToCount[bestmoveBestMoves[i]];
                        }
                        List<KeyValuePair<string, int>> bestmoveAndCount = new List<KeyValuePair<string, int>>(bestmoveToCount);
                        bestmoveAndCount.Sort((KeyValuePair<string, int> lh, KeyValuePair<string, int> rh) => { return -(lh.Value - rh.Value); });

                        string vote = "info string";
                        foreach (var p in bestmoveAndCount)
                        {
                            vote += " ";
                            vote += p.Key;
                            vote += "=";
                            vote += p.Value;
                        }
                        WriteLineAndFlush(Console.Out, vote);

                        // info pvの表示
                        WriteLineAndFlush(Console.Out, Join(command));
                        depth = tempDepth;
                    }
                }
            }

            private void HandleBestmove(List<string> command)
            {
                if (command[1] == "resign" || command[1] == "win")
                {
                    for (int i = 0; i < bestmoveBestMoves.Length; ++i)
                    {
                        bestmoveBestMoves[i] = command[1];
                        bestmovePonders[i] = null;
                    }
                    if (command.Count == 4 && command[2] == "ponder")
                    {
                        for (int i = 0; i < bestmoveBestMoves.Length; ++i)
                        {
                            bestmovePonders[i] = command[3];
                        }
                    }
                }

                // 手番かつ他の思考エンジンがbestmoveを返していない時のみ
                // bestmoveを返すようにする
                lock (upstreamLockObject)
                {
                    if (upstreamState == UpstreamState.Stopped)
                    {
                        //Debug.WriteLine("     ## process={0} upstreamState == UpstreamState.Stopped", process);
                        return;
                    }

                    lock (downstreamLockObject)
                    {
                        // 思考中の局面が違う場合は処理しない
                        if (upstreamPosition != downstreamPosition)
                        {
                            Debug.WriteLine("  ## process={0} upstreamGoIndex != downstreamGoIndex", process);
                            return;
                        }
                    }

                    if (bestmoveBestMoves == null)
                    {
                        return;
                    }

                    if (bestmovePonders == null)
                    {
                        return;
                    }

                    Dictionary<string, int> bestmoveToCount = new Dictionary<string, int>();
                    for (int i = 0; i < bestmoveBestMoves.Length; ++i)
                    {
                        if (bestmoveBestMoves[i] == null)
                        {
                            continue;
                        }
                        if (!bestmoveToCount.ContainsKey(bestmoveBestMoves[i]))
                        {
                            bestmoveToCount.Add(bestmoveBestMoves[i], 0);
                        }
                        ++bestmoveToCount[bestmoveBestMoves[i]];
                    }

                    int bestCount = -1;
                    string bestmove = "resign";
                    foreach (var p in bestmoveToCount)
                    {
                        if (p.Value > bestCount)
                        {
                            bestCount = p.Value;
                            bestmove = p.Key;
                        }
                    }

                    int ponderCount = 0;
                    string ponder = null;
                    for (int i = 0; i < bestmoveBestMoves.Length; ++i)
                    {
                        if (bestmoveBestMoves[i] != bestmove)
                        {
                            continue;
                        }
                        if (bestmovePonders[i] == null)
                        {
                            continue;
                        }
                        ++ponderCount;
                        if (random.Next(ponderCount) == 0)
                        {
                            ponder = bestmovePonders[i];
                        }
                    }

                    string outputCommand = null;
                    if (!string.IsNullOrEmpty(ponder))
                    {
                        outputCommand = string.Format("bestmove {0} ponder {1}", bestmove, ponder);
                    }
                    else
                    {
                        outputCommand = string.Format("bestmove {0}", bestmove);
                    }

                    Debug.WriteLine("  <<    engine={0} command={1}", name, outputCommand);
                    WriteLineAndFlush(Console.Out, outputCommand);

                    TransitUpstreamState(UpstreamState.Stopped);
                    depth = 0;
                    bestmoveBestMoves = null;
                    bestmovePonders = null;
                    WriteToEachEngine("stop");
                }
            }
        }

        static List<Engine> engines = new List<Engine>();

        static int GetProcessId()
        {
            using (Process process = Process.GetCurrentProcess())
            {
                return process.Id;
            }
        }

        static void Main(string[] args)
        {
            //writeSampleSetting();
            ProxySetting setting = loadSetting();
            Debug.Listeners.Add(new TextWriterTraceListener(Console.Error));
            string logFileFormat = setting.logDirectory + "\\" + DateTime.Now.ToString("yyyy-MM-dd-HH-mm-ss") + string.Format("_pid={0}", GetProcessId()) + ".txt";
            Debug.Listeners.Add(new TextWriterTraceListener(new StreamWriter(logFileFormat, false, Encoding.UTF8)));


            for (int id = 0; id < setting.engines.Length; ++id)
            {
                engines.Add(new Engine(setting.engines[id], id));
            }

            // 子プロセスの標準入出力 (System.Diagnostics.Process) - Programming/.NET Framework/標準入出力 - 総武ソフトウェア推進所 http://smdn.jp/programming/netfx/standard_streams/1_process/
            try
            {
                foreach (var engine in engines)
                {
                    engine.RunAsync();
                }

                string input;
                while ((input = Console.ReadLine()) != null)
                {
                    var command = Split(input);

                    if (command[0] == "go")
                    {
                        // 思考開始の合図です。エンジンはこれを受信すると思考を開始します。
                        lock (upstreamLockObject)
                        {
                            bestmoveBestMoves = new string[engines.Count];
                            bestmovePonders = new string[engines.Count];
                            depth = 0;
                            TransitUpstreamState(UpstreamState.Thinking);
                            Console.WriteLine("info string " + upstreamPosition);
                            Console.Out.Flush();
                        }
                    }
                    else if (command[0] == "isready")
                    {
                        lock (upstreamLockObject)
                        {
                            numberOfReadyoks = 0;
                        }
                    }
                    else if (command[0] == "position")
                    {
                        lock (upstreamLockObject)
                        {
                            upstreamPosition = input;
                            Debug.WriteLine("upstreamPosition=" + upstreamPosition);
                        }
                    }

                    WriteToEachEngine(input);

                    if (input == "quit")
                    {
                        break;
                    }
                }
            }
            finally
            {
                Debug.Flush();//すべての出力をファイルに書き込むのに必要
                foreach (var engine in engines)
                {
                    engine.Close();
                }
            }
        }

        /// <summary>
        /// エンジンに対して出力する
        /// </summary>
        /// <param name="input">親ソフトウェアからの入力。USIプロトコルサーバーまたは親tanuki-proxy</param>
        private static void WriteToEachEngine(string input)
        {
            foreach (var engine in engines)
            {
                engine.Write(input);
                if (input == "usi")
                {
                    break;
                }
            }
        }

        static List<string> Split(string s)
        {
            return new List<string>(new Regex("\\s+").Split(s));
        }

        static void TransitUpstreamState(UpstreamState newUpstreamState)
        {
            Debug.WriteLine("  || upstream {0} > {1}", upstreamState, newUpstreamState);
            upstreamState = newUpstreamState;
        }

        static void writeSampleSetting()
        {
            ProxySetting setting = new ProxySetting();
            setting.logDirectory = "C:\\home\\develop\\tanuki-";
            setting.engines = new EngineOption[]
            {
            new EngineOption(
                "doutanuki",
                "C:\\home\\develop\\tanuki-\\tanuki-\\x64\\Release\\tanuki-.exe",
                "",
                "C:\\home\\develop\\tanuki-\\bin",
                new[] {
                    new Option("USI_Hash", "1024"),
                    new Option("Book_File", "../bin/book-2016-02-01.bin"),
                    new Option("Best_Book_Move", "true"),
                    new Option("Max_Random_Score_Diff", "0"),
                    new Option("Max_Random_Score_Diff_Ply", "0"),
                    new Option("Threads", "1"),
                }),
            new EngineOption(
                "nighthawk",
                "ssh",
                "-vvv nighthawk tanuki-.bat",
                "C:\\home\\develop\\tanuki-\\bin",
                new[] {
                    new Option("USI_Hash", "1024"),
                    new Option("Book_File", "../bin/book-2016-02-01.bin"),
                    new Option("Best_Book_Move", "true"),
                    new Option("Max_Random_Score_Diff", "0"),
                    new Option("Max_Random_Score_Diff_Ply", "0"),
                    new Option("Threads", "4"),
                }),
            new EngineOption(
                "doutanuki",
                "C:\\home\\develop\\tanuki-\\tanuki-\\x64\\Release\\tanuki-.exe",
                "",
                "C:\\home\\develop\\tanuki-\\bin",
                new[] {
                    new Option("USI_Hash", "1024"),
                    new Option("Book_File", "../bin/book-2016-02-01.bin"),
                    new Option("Best_Book_Move", "true"),
                    new Option("Max_Random_Score_Diff", "0"),
                    new Option("Max_Random_Score_Diff_Ply", "0"),
                    new Option("Threads", "1"),
                })
            };

            DataContractJsonSerializer serializer = new DataContractJsonSerializer(typeof(ProxySetting));
            using (FileStream f = new FileStream("proxy-setting.sample.json", FileMode.Create))
            {
                serializer.WriteObject(f, setting);
            }
        }

        static string ExeDir()
        {
            using (Process process = Process.GetCurrentProcess())
            {
                return Path.GetDirectoryName(process.MainModule.FileName);
            }
        }

        static ProxySetting loadSetting()
        {
            DataContractJsonSerializer serializer = new DataContractJsonSerializer(typeof(ProxySetting));
            // 1. まずはexeディレクトリに設定ファイルがあれば使う(複数Proxy設定をexeごとディレクトリに分け、カレントディレクトリは制御できない場合)
            // 2. それが無ければ、カレントディレクトリの設定を使う
            string[] search_dirs = { ExeDir(), "." };
            foreach (string search_dir in search_dirs)
            {
                string path = Path.Combine(search_dir, "proxy-setting.json");
                if (File.Exists(path))
                {
                    using (FileStream f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
                    {
                        return (ProxySetting)serializer.ReadObject(f);
                    }
                }
            }
            return null;
        }
    }
}
