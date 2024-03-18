/*
 *  HDRMerge - HDR exposure merging software.
 *  Copyright 2012 Javier Celaya
 *  jcelaya@gmail.com
 *
 *  This file is part of HDRMerge.
 *
 *  HDRMerge is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  HDRMerge is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with HDRMerge. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <exiv2/exiv2.hpp>
#include <iostream>
#include "ExifTransfer.hpp"
#include "Log.hpp"

namespace hdrmerge {

class ExifTransfer {
public:
    ExifTransfer(const QString & srcFile, const QString & dstFile,
                 const uint8_t * data, size_t dataSize)
    : srcFile(srcFile), dstFile(dstFile), data(data), dataSize(dataSize) {}

    void copyMetadata();

private:
    QString srcFile, dstFile;
    const uint8_t * data;
    size_t dataSize;
    std::unique_ptr<Exiv2::Image> src, dst;

    void copyXMP();
    void copyIPTC();
    void copyEXIF();
};


void hdrmerge::Exif::transfer(const QString & srcFile, const QString & dstFile,
                 const uint8_t * data, size_t dataSize) {
    ExifTransfer exif(srcFile, dstFile, data, dataSize);
    exif.copyMetadata();
}


void ExifTransfer::copyMetadata() {
    try {
        // .reset(.release()) accounts for old versions of Exiv2 that return an auto_ptr
        dst.reset(Exiv2::ImageFactory::open(data, dataSize).release());
        dst->readMetadata();
    } catch (Exiv2::Error & e) {
        std::cerr << "Exiv2 error: " << e.what() << std::endl;
        return;
    }
    try {
        src.reset(Exiv2::ImageFactory::open(srcFile.toLocal8Bit().constData()).release());
        src->readMetadata();
        copyXMP();
        copyIPTC();
        copyEXIF();
    } catch (Exiv2::Error & e) {
        std::cerr << "Exiv2 error: " << e.what() << std::endl;
        // At least we have to set the SubImage1 file type to Primary Image
        dst->exifData()["Exif.SubImage1.NewSubfileType"] = 0;
    }
    try {
        dst->writeMetadata();
        Exiv2::FileIo fileIo(dstFile.toLocal8Bit().constData());
        fileIo.open("wb");
        fileIo.write(dst->io());
        fileIo.close();
    } catch (Exiv2::Error & e) {
        std::cerr << "Exiv2 error: " << e.what() << std::endl;
    }
}


void ExifTransfer::copyXMP() {
    const Exiv2::XmpData & srcXmp = src->xmpData();
    Exiv2::XmpData & dstXmp = dst->xmpData();
    for (const auto & datum : srcXmp) {
        if (datum.groupName() != "tiff" && dstXmp.findKey(Exiv2::XmpKey(datum.key())) == dstXmp.end()) {
            dstXmp.add(datum);
        }
    }
}


void ExifTransfer::copyIPTC() {
    const Exiv2::IptcData & srcIptc = src->iptcData();
    Exiv2::IptcData & dstIptc = dst->iptcData();
    for (const auto & datum : srcIptc) {
        if (dstIptc.findKey(Exiv2::IptcKey(datum.key())) == dstIptc.end()) {
            dstIptc.add(datum);
        }
    }
}


static bool excludeExifDatum(const Exiv2::Exifdatum & datum) {
    static const char * previewKeys[] {
        "Exif.OlympusCs.PreviewImageStart",
        "Exif.OlympusCs.PreviewImageLength",
        "Exif.Thumbnail.JPEGInterchangeFormat",
        "Exif.Thumbnail.JPEGInterchangeFormatLength",
        "Exif.NikonPreview.JPEGInterchangeFormat",
        "Exif.NikonPreview.JPEGInterchangeFormatLength",
        "Exif.Pentax.PreviewOffset",
        "Exif.Pentax.PreviewLength",
        "Exif.PentaxDng.PreviewOffset",
        "Exif.PentaxDng.PreviewLength",
        "Exif.Minolta.ThumbnailOffset",
        "Exif.Minolta.ThumbnailLength",
        "Exif.SonyMinolta.ThumbnailOffset",
        "Exif.SonyMinolta.ThumbnailLength",
        "Exif.Olympus.ThumbnailImage",
        "Exif.Olympus2.ThumbnailImage",
        "Exif.Minolta.Thumbnail",
        "Exif.PanasonicRaw.PreviewImage",
        "Exif.SamsungPreview.JPEGInterchangeFormat",
        "Exif.SamsungPreview.JPEGInterchangeFormatLength"
    };
    for (const char * pkey : previewKeys) {
        if (datum.key() == pkey) {
            return true;
        }
    }
    return
        datum.groupName().substr(0, 5) == "Thumb" ||
        datum.groupName().substr(0, 8) == "SubThumb" ||
        datum.groupName().substr(0, 5) == "Image" ||
        datum.groupName().substr(0, 8) == "SubImage";
}


void ExifTransfer::copyEXIF() {
    static const char * includeImageKeys[] = {
        // Correct Make and Model, from the input files
        // It is needed so that makernote tags are correctly copied
        "Exif.Image.Make",
        "Exif.Image.Model",
        "Exif.Image.Artist",
        "Exif.Image.Copyright",
        "Exif.Image.DNGPrivateData",
        // Opcodes generated by Adobe DNG converter
        "Exif.SubImage1.OpcodeList1",
        "Exif.SubImage1.OpcodeList2",
        "Exif.SubImage1.OpcodeList3"
    };

    const Exiv2::ExifData & srcExif = src->exifData();
    Exiv2::ExifData & dstExif = dst->exifData();

    for (const char * keyName : includeImageKeys) {
        auto iterator = srcExif.findKey(Exiv2::ExifKey(keyName));
        if (iterator != srcExif.end()) {
            dstExif[keyName] = *iterator;
        }
    }
    // Now we set the SubImage1 file type to Primary Image
    // Exiv2 wouldn't modify SubImage1 tags if it was set before
    dstExif["Exif.SubImage1.NewSubfileType"] = 0u;

    for (const auto & datum : srcExif) {
        if (!excludeExifDatum(datum) && dstExif.findKey(Exiv2::ExifKey(datum.key())) == dstExif.end()) {
            dstExif.add(datum);
        }
    }
}

}
