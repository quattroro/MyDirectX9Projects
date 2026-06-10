using UnrealCli.Cli;

using var cts = new CancellationTokenSource();
Console.CancelKeyPress += (_, e) => { e.Cancel = true; cts.Cancel(); };

int exitCode = await CliApp.RunAsync(args, cts.Token);
return exitCode;
