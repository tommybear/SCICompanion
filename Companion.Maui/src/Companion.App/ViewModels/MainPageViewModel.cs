using System.Collections.ObjectModel;
using Companion.Domain.Assets;

namespace Companion.App.ViewModels;

public sealed class MainPageViewModel
{
    public ObservableCollection<SampleAsset> Assets { get; }

    public MainPageViewModel(ISampleAssetCatalog catalog)
    {
        Assets = new ObservableCollection<SampleAsset>(catalog.GetAssets());
    }
}
