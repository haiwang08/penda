using System.Security.AccessControl;
using System.Security.Principal;
namespace Penda.Core;

public static class RuntimeDictionary
{
    public static string Root => Path.Combine(Storage.Root,"Runtime");
    // Only this read-only cache is exposed to app containers, never the editable source dictionaries.
    public static void Publish(string source)
    {
        if(!OperatingSystem.IsWindows())return;
        Directory.CreateDirectory(Root);
        var folder=new DirectoryInfo(Root);var acl=FileSystemAclExtensions.GetAccessControl(folder);
        foreach(var sid in new[]{"S-1-15-2-1","S-1-15-2-2"})acl.AddAccessRule(new FileSystemAccessRule(new SecurityIdentifier(sid),FileSystemRights.ReadAndExecute,InheritanceFlags.ContainerInherit|InheritanceFlags.ObjectInherit,PropagationFlags.None,AccessControlType.Allow));
        FileSystemAclExtensions.SetAccessControl(folder,acl);
        var target=Path.Combine(Root,"engine.pdat");var temporary=target+"."+Environment.ProcessId+".tmp";
        File.Copy(source,temporary,true);File.Move(temporary,target,true);
        var file=new FileInfo(target);var access=FileSystemAclExtensions.GetAccessControl(file);
        foreach(var sid in new[]{"S-1-15-2-1","S-1-15-2-2"})access.AddAccessRule(new FileSystemAccessRule(new SecurityIdentifier(sid),FileSystemRights.ReadAndExecute,AccessControlType.Allow));
        FileSystemAclExtensions.SetAccessControl(file,access);
    }
}
