using System.Diagnostics;
using System.Runtime.InteropServices;

namespace HostingOsx;

public class Class1
{
    [UnmanagedCallersOnly]
    public static void EntryPoint()
    {
        Console.WriteLine("waiting for dotnet debugger");

        while (!Debugger.IsAttached)
            Thread.Sleep(200);

        Console.WriteLine("dotnet entry point reached");
    }
}
