using System;
using System.Linq;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Application.Tests.Support;
using Companion.Domain.Compression;
using Companion.Domain.Projects;
using Companion.Domain.Resources;
using Xunit;

namespace Companion.Application.Tests;

public class CompressionFixtureTests
{
    [Fact]
    public void Sci0_Pic_MatchesFixture()
    {
        var (data, method) = DecompressResource("SCI0", ResourceType.Pic, 1);
        var expected = Convert.FromBase64String("/gEAABEiM0RVZneImaq7zN3uiIgBAgMEBQaIiPn6+/z9/v8IkSo7TF1uiP4BAQABAgMEBQYHCIGCg4SF5oeHcXJzdHV2eIeJiouMjY6P+PH6O/z9/vj+AQIAURIjJGXGNwhZGissbc64iNGSo6RlRjh42ZqrrG1OP3+U2uu8TVSI/gEDYGFiY2RlZmfo6err7O3u6EhBQkNERUZIyMnKy8zNzs8YERoLHB0eGPkA8fP8/wAwEyU=");
        Assert.Equal(0, method);
        Assert.Equal(expected, data);
    }

    [Fact]
    public void Sci11_Pic_MatchesFixture()
    {
        var (data, method) = DecompressResource("SCI1.1", ResourceType.Pic, 0);
        var expected = Convert.FromBase64String("JgAQDgAAoADw2EYACQAAAF0AAAAAAAAAAAAAAEoAAABEAAAAAAAAACoANQBAAEoAVQBfAGoAdAB/AIoAlACfAKkAtAAAAw0AAAAMAPjR4IlG+rgBAAAAAAUJAAAA8ADz/PgAcmb/");
        Assert.Equal(20, method);
        Assert.Equal(expected, data);
    }

    [Fact]
    public void Sci11_Text10_Dcl18MatchesFixture()
    {
        var (data, method) = DecompressResource("SCI1.1", ResourceType.Text, 10);
        var expected = Convert.FromBase64String("JWQAY2xhc3M6ICVzCnZpZXc6ICVkCmxvb3A6ICVkCmNlbDogJWQKcG9zbjogJWQgJWQgJWQKaGVhZGluZzogJWQKcHJpOiAlZApzaWduYWw6ICQleAppbGxCaXRzOiAkJXgKAG5hbWU6ICVzCnZpZXc6ICVkCmxvb3A6ICVkCmNlbDogJWQKcG9zbjogJWQgJWQgJWQKaGVhZGluZzogJWQKcHJpOiAlZApzaWduYWw6ICQleAppbGxCaXRzOiAkJXgKT25Db250cm9sOiAkJXgKT3JpZ2luIG9uOiAkJXggAG5hbWU6ICVzCm51bWJlcjogJWQKY3VycmVudCBwaWM6ICVkCnN0eWxlOiAlZApob3Jpem9uOiAlZApub3J0aDogJWQKc291dGg6ICVkCmVhc3Q6ICVkCndlc3Q6ICVkCnNjcmlwdDogJXMgACVkLyVkAA==");
        Assert.Equal(18, method);
        Assert.Equal(expected, data);
    }

    [Fact]
    public void Sci11_View981_Dcl19MatchesFixture()
    {
        var (data, method) = DecompressResource("SCI1.1", ResourceType.View, 981);
        var expected = Convert.FromBase64String("EAABAAEAAQAAAAAAECQAAAAA/wAB/////wMAAAAAIgAAABEACAADAAQAPwoAAGIAAAAjAAAAAAAAAEwAAABvAAAAAAAAAAAEYgAAAMSGx8IChQPFBYMFxAaDggPDBYSDAsMFgwbDBYQExMOChILGBgUEAAMEBj8FAwAFAAQAAAQFAwAHBQYHAAUDAAQAAwUFAwAFAwUEBQMDBQAFAwMABQQ/BAUEAAMABAUEBAUE");
        Assert.Equal(19, method);
        Assert.Equal(expected, data);
    }

    private static (byte[] Data, int Method) DecompressResource(string folderName, ResourceType type, int number)
    {
        var discovery = new ResourceDiscoveryService();
        var reader = new ResourceVolumeReader();
        var registry = BuildCompressionRegistry();

        var folder = RepoPaths.GetTemplateGame(folderName);
        var catalog = discovery.Discover(folder);
        var descriptor = catalog.Resources.First(r => r.Type == type && r.Number == number);
        var package = reader.Read(folder, descriptor, catalog.Version);
        var method = package.Header.CompressionMethod;
        var data = method == 0
            ? package.Body
            : registry.Decompress(package.Body, method, package.Header.DecompressedLength);
        return (data, method);
    }

    private static CompressionRegistry BuildCompressionRegistry()
    {
        var lzw1 = new Lzw1CompressionService(2);
        return new CompressionRegistry(new ICompressionService[]
        {
            new PassthroughCompressionService(0),
            new LzwCompressionService(1),
            lzw1,
            new LzwViewCompressionService(lzw1, 3),
            new LzwPicCompressionService(lzw1, 4),
            new DclCompressionService(8, 18, 19, 20),
            new StacCompressionService(32)
        });
    }
}
