/***************************************************************************
    Copyright (c) 2015 Philip Fortier

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
***************************************************************************/
#include "stdafx.h"
#include "AudioMap.h"
#include "ResourceEntity.h"
#include "format.h"
#include "ResourceSourceFlags.h"

using namespace std;

void AudioMapWriteTo(const ResourceEntity &resource, sci::ostream &byteStream, std::map<BlobKey, uint32_t> &propertyBag)
{
    uint8_t ff[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    const AudioMapComponent &map = resource.GetComponent<AudioMapComponent>();
    if (map.Version == AudioMapVersion::FiveBytes)
    {
        uint32_t prevOffset = 0;
        for (const AudioMapEntry &entry : map.Entries)
        {
            byteStream << entry.Number;
            uint32_t cumOffset = entry.Offset - prevOffset;
            byteStream.WriteWord((uint16_t)cumOffset);
            byteStream.WriteByte((uint8_t)(cumOffset >> 16));
            prevOffset = entry.Offset;
        }
        byteStream.FillByte(0xff, 5);
    }
    else if (map.Version == AudioMapVersion::SixBytes)
    {
        for (const AudioMapEntry &entry : map.Entries)
        {
            byteStream << entry.Number;
            byteStream << entry.Offset;
        }
        byteStream.FillByte(0xff, 6);
    }
    else if (map.Version == AudioMapVersion::EightBytes)
    {
        for (const AudioMapEntry &entry : map.Entries)
        {
            byteStream << entry.Number;
            uint16_t ffff = 0xffff;
            byteStream << ffff;
            byteStream << entry.Offset;
        }
        byteStream.FillByte(0xff, 8);
    }
    else if (map.Version == AudioMapVersion::SyncMapEarly)
    {
        for (const AudioMapEntry &entry : map.Entries)
        {
            byteStream << entry.Noun;
            byteStream << entry.Verb;
            byteStream << entry.Condition;
            byteStream << entry.Sequence;
            byteStream << entry.Offset;
            byteStream << entry.SyncSize;
        }
        byteStream.FillByte(0xff, 10);
    }
    else if (map.Version == AudioMapVersion::SyncMapLate)
    {
        // Offsets must be ever increasing. That limits sounds to 16 MB. I guess LB2 uses the old version which doesn't?
        uint32_t cumulativeOffset = 0;
        if (!map.Entries.empty())
        {
            cumulativeOffset = map.Entries[0].Offset;
            byteStream << cumulativeOffset;
        }
        for (const AudioMapEntry &entry : map.Entries)
        {
            byteStream << entry.Noun;
            byteStream << entry.Verb;
            byteStream << entry.Condition;
            uint8_t seq = entry.Sequence;
            if (entry.SyncSize > 0)
            {
                seq |= 0x80;    // Flag indicating we have a sync size
            }
            byteStream << seq;
            // There is also 0x40 for raw lip sync data, but we don't support that.

            if (resource.SourceFlags == ResourceSourceFlags::AudioMapCache)
            {
                byteStream.FillByte(0, 3); // Don't care if this is the cache file.
            }
            else
            {
                assert(entry.Offset >= cumulativeOffset);   // We should never allow this to happen
                uint32_t delta = entry.Offset - cumulativeOffset;
                cumulativeOffset = entry.Offset;
                // This should have been validated.
                assert(delta < (256 * 256 * 256));
                uint8_t bytes[3] = { (uint8_t)(0xff & delta), (uint8_t)(0xff & (delta >> 8)), (uint8_t)(0xff & (delta >> 16)) };
                byteStream.WriteBytes(bytes, sizeof(bytes));
            }

            if (entry.SyncSize > 0)
            {
                byteStream << entry.SyncSize;
            }
        }
        byteStream.FillByte(0xff, 11);
    }
    else
    {
        assert(false);
    }
}

AudioMapVersion _DetermineAudioMapVersion(int resourceNumber, int mainAudioMapResourceNumber, sci::istream &stream)
{
    // Detect if it's 5 or 6 byte entries
    int ffCount = 0;
    const uint8_t *data = stream.GetInternalPointer() + stream.GetDataSize() - 1;
    for (size_t i = 0; i < stream.GetDataSize(); i++)
    {
        if (*data == 0xff)
        {
            ffCount++;
            data--;
        }
        else
        {
            break;
        }
    }

    if (mainAudioMapResourceNumber == resourceNumber)
    {
        if (resourceNumber == 65535)
        {
            if (ffCount >= 6)
            {
                return AudioMapVersion::SixBytes;
            }
            else if (ffCount == 5)
            {
                return AudioMapVersion::FiveBytes;
            }
        }
        else if (resourceNumber == 0)
        {
            // LB2 (non-CD), and apparently Mother Goose SCI 1.1 remake.
            if (ffCount >= 8)
            {
                return AudioMapVersion::EightBytes;
            }
        }
    }
    else
    {
        // This is a map with verb/noun/cond/seq sync information.
        // It maps a message tuple to an audio (speech) resource, along with possible sync info.
        if (ffCount >= 11)
        {
            return AudioMapVersion::SyncMapLate;
        }
        else if (ffCount >= 10)
        {
            return AudioMapVersion::SyncMapEarly;
        }
    }

    throw std::exception("Unknown audio map format");
}

std::pair<AudioMapVersion, uint32_t> AudioMapVersionByteCounts[] =
{
    { AudioMapVersion::None, 0 },
    { AudioMapVersion::FiveBytes, 5},
    { AudioMapVersion::SixBytes, 6 },
    { AudioMapVersion::EightBytes, 8 },
    { AudioMapVersion::SyncMapEarly, 10 },
    { AudioMapVersion::SyncMapLate, 11 },
};

uint32_t GetEntryByteCount(AudioMapVersion mapVersion)
{
    auto it = find_if(begin(AudioMapVersionByteCounts), end(AudioMapVersionByteCounts), [&mapVersion](pair<AudioMapVersion, uint32_t> &entry) { return mapVersion == entry.first; });
    return it->second;
}

void AudioMapReadFrom(ResourceEntity &resource, sci::istream &stream, const std::map<BlobKey, uint32_t> &propertyBag)
{
    AudioMapComponent &map = resource.GetComponent<AudioMapComponent>();

    map.Version = _DetermineAudioMapVersion(resource.ResourceNumber, map.Traits.MainAudioMapResourceNumber, stream);

    uint32_t cumOffset = 0;
    if (map.Version == AudioMapVersion::SyncMapLate)
    {
        stream >> cumOffset;
    }

    while (stream.getBytesRemaining() > GetEntryByteCount(map.Version))
    {
        AudioMapEntry entry = { };
        switch (map.Version)
        {
            case AudioMapVersion::SixBytes:
            {
                stream >> entry.Number;
                stream >> entry.Offset;
            }
                break;
            case AudioMapVersion::FiveBytes:
            {
                stream >> entry.Number;
                // 24 bit value that is a cumulative offset
                uint16_t low;
                uint8_t high;
                stream >> low;
                stream >> high;
                uint32_t add = low + (high << 16);
                cumOffset += add;
                entry.Offset = cumOffset;
            }
                break;

            case AudioMapVersion::EightBytes:
            {
                stream >> entry.Number;
                stream.skip(2); // This should be 0xffff
                stream >> entry.Offset;
            }
                break;

            case AudioMapVersion::SyncMapEarly:
            {
                stream >> entry.Noun;
                stream >> entry.Verb;
                stream >> entry.Condition;
                stream >> entry.Sequence;
                stream >> entry.Offset;
                stream >> entry.SyncSize;
                entry.Sequence &= 0x3f; // Top two bits not used
                entry.Number = resource.ResourceNumber;
            }
                break;

            case AudioMapVersion::SyncMapLate:
            {
                stream >> entry.Noun;
                stream >> entry.Verb;
                stream >> entry.Condition;
                stream >> entry.Sequence;
                // 3 bytes
                uint8_t bytes[3];
                stream.read_data(bytes, sizeof(bytes));
                uint32_t offset = bytes[0] + (bytes[1] << 8) + (bytes[2] << 16);
                cumOffset += offset;
                entry.Offset = cumOffset;

                if (entry.Sequence & 0x80)
                {
                    stream >> entry.SyncSize;
                }
                if (entry.Sequence & 0x40)
                {
                    uint16_t rawLipSyncDataSize;
                    stream >> rawLipSyncDataSize;
                    // REVIEW: This is bogus, but good enough for now (until we support raw lip sync data)
                    entry.SyncSize += rawLipSyncDataSize;
                }
                entry.Sequence &= 0x3f; // Top two bits not used, except as above
                entry.Number = resource.ResourceNumber;
            }
                break;

            default:
                assert(false);
                break;
        }

        switch (map.Version)
        {
            case AudioMapVersion::FiveBytes:
            case AudioMapVersion::SixBytes:
            case AudioMapVersion::EightBytes:
                if (entry.Number != 0xffff)
                {
                    map.Entries.push_back(entry);
                }
                break;

            default:
                if ((entry.Noun != 0xff) || (entry.Verb != 0xff) || (entry.Condition != 0xff))
                {
                    map.Entries.push_back(entry);
                }
                break;
        }

    }
}

bool ValidateAudioMap(const ResourceEntity &resource)
{
    if (resource.SourceFlags == ResourceSourceFlags::AudioMapCache)
    {
        // Offsets may not be valid in this case, so we don't need to worry.
        return true;
    }

    const AudioMapComponent &map = resource.GetComponent<AudioMapComponent>();
    if (map.Version == AudioMapVersion::SyncMapLate)
    {
        uint32_t cumulativeOffset = 0;
        if (!map.Entries.empty())
        {
            cumulativeOffset = map.Entries[0].Offset;
        }
        for (const AudioMapEntry &entry : map.Entries)
        {
            if ((entry.Offset - cumulativeOffset) > (256 * 256 * 256))
            {
                std::string message = fmt::format("N:{0}, V:{1}, C:{2}, S:{3} is tool large. It needs to be less than 16MB.", (int)entry.Noun, (int)entry.Verb, (int)entry.Condition, (int)entry.Sequence);
                AfxMessageBox(message.c_str(), MB_ICONWARNING | MB_OK);
                return false;
            }
            cumulativeOffset = entry.Offset;
        }
    }
    return true;
}

ResourceTraits audioMapTraits =
{
    ResourceType::AudioMap,
    &AudioMapReadFrom,
    &AudioMapWriteTo,
    &ValidateAudioMap,
    nullptr
};

bool IsMainAudioMap(AudioMapVersion version)
{
    return version <= AudioMapVersion::EightBytes;
}

AudioMapTraits audioMapTraits0 =
{ 0 };

AudioMapTraits audioMapTraits65535 =
{ 65535 };

AudioMapComponent::AudioMapComponent() : AudioMapComponent(audioMapTraits65535) {}
AudioMapComponent::AudioMapComponent(const AudioMapTraits &traits) : Traits(traits) {}

ResourceEntity *CreateMapResource(SCIVersion version)
{
    AudioMapTraits *mapTraits = &audioMapTraits0;
    if (version.AudioMapResourceNumber == 65535)
    {
        mapTraits = &audioMapTraits65535;
    }
    else if (version.AudioMapResourceNumber == 0)
    {
        mapTraits = &audioMapTraits0;
    }
    else
    {
        throw std::exception("Unknown audio map resource number.");
    }

    std::unique_ptr<ResourceEntity> pResource = std::make_unique<ResourceEntity>(audioMapTraits);
    pResource->AddComponent(move(make_unique<AudioMapComponent>(*mapTraits)));
    return pResource.release();
}

ResourceEntity *CreateDefaultMapResource(SCIVersion version, int number)
{
    ResourceEntity *resource = CreateMapResource(version);
    resource->GetComponent<AudioMapComponent>().Version = (number == version.AudioMapResourceNumber) ? version.MainAudioMapVersion : version.Base36AudioMapVersion;
    resource->Base36Number = NoBase36;
    resource->PackageNumber = version.DefaultVolumeFile;
    resource->ResourceNumber = number;
    return resource;
}