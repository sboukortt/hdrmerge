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

#include <iostream>
#include <iomanip>
#include <string>
#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#include <QLocale>
#include "Launcher.hpp"
#include "ImageIO.hpp"
#ifndef NO_GUI
#include "MainWindow.hpp"
#endif
#include "Log.hpp"
#include <libraw.h>

namespace hdrmerge {

Launcher::Launcher(int argc, char * argv[]) : argc(argc), argv(argv), help(false) {
    Log::setOutputStream(std::cout);
    saveOptions.previewSize = 2;
}


int Launcher::startGUI() {
#ifndef NO_GUI
    // Create main window
    MainWindow mw;
    mw.preload(generalOptions.fileNames);
    mw.show();
    QMetaObject::invokeMethod(&mw, "loadImages", Qt::QueuedConnection);

    return QApplication::exec();
#else
    return 0;
#endif
}


struct CoutProgressIndicator : public ProgressIndicator {
    virtual void advance(int percent, const char * message, const char * arg) {
        if (arg) {
            Log::progress('[', std::setw(3), percent, "%] ", QCoreApplication::translate("LoadSave", message).arg(arg));
        } else {
            Log::progress('[', std::setw(3), percent, "%] ", QCoreApplication::translate("LoadSave", message));
        }
    }
};


std::list<LoadOptions> Launcher::getBracketedSets() {
    std::list<LoadOptions> result;
    std::list<std::pair<ImageIO::QDateInterval, QString>> dateNames;
    for (QString & name : generalOptions.fileNames) {
        ImageIO::QDateInterval interval = ImageIO::getImageCreationInterval(name);
        if (interval.start.isValid()) {
            dateNames.emplace_back(interval, name);
        } else {
            // We cannot get time information, process it alone
            result.push_back(generalOptions);
            result.back().fileNames.clear();
            result.back().fileNames.push_back(name);
        }
    }
    dateNames.sort();
    ImageIO::QDateInterval lastInterval;
    for (auto & dateName : dateNames) {
        if (lastInterval.start.isNull() || lastInterval.difference(dateName.first) > generalOptions.batchGap) {
            result.push_back(generalOptions);
            result.back().fileNames.clear();
        }
        result.back().fileNames.push_back(dateName.second);
        lastInterval = dateName.first;
    }
    int setNum = 0;
    for (auto & i : result) {
        Log::progressN("Set ", setNum++, ":");
        for (auto & j : i.fileNames) {
            Log::progressN(" ", j);
        }
        Log::progress();
    }
    return result;
}


int Launcher::automaticMerge() {
    auto tr = [&] (const char * text) { return QCoreApplication::translate("LoadSave", text); };
    std::list<LoadOptions> optionsSet;
    if (generalOptions.batch) {
        optionsSet = getBracketedSets();
    } else {
        optionsSet.push_back(generalOptions);
    }
    ImageIO io;
    int result = 0;
    for (LoadOptions & options : optionsSet) {
        if (!options.withSingles && options.fileNames.size() == 1) {
            Log::progress(tr("Skipping single image %1").arg(options.fileNames.front()));
            continue;
        }
        CoutProgressIndicator progress;
        int numImages = options.fileNames.size();
        int result = io.load(options, progress);
        if (result < numImages * 2) {
            int format = result & 1;
            int i = result >> 1;
            if (format) {
                std::cerr << tr("Error loading %1, it has a different format.").arg(options.fileNames[i]) << std::endl;
            } else {
                std::cerr << tr("Error loading %1, file not found.").arg(options.fileNames[i]) << std::endl;
            }
            result = 1;
            continue;
        }
        SaveOptions setOptions = saveOptions;
        if (!setOptions.fileName.isEmpty()) {
            setOptions.fileName = io.replaceArguments(setOptions.fileName, "");
            int extPos = setOptions.fileName.lastIndexOf('.');
            if (extPos > setOptions.fileName.length() || setOptions.fileName.mid(extPos) != ".dng") {
                setOptions.fileName += ".dng";
            }
        } else {
            setOptions.fileName = io.buildOutputFileName();
        }
        Log::progress(tr("Writing result to %1").arg(setOptions.fileName));
        io.save(setOptions, progress);
    }
    return result;
}


void Launcher::parseCommandLine() {
    auto tr = [&] (const char * text) { return QCoreApplication::translate("Help", text); };
    for (int i = 1; i < argc; ++i) {
        if (std::string("-o") == argv[i]) {
            if (++i < argc) {
                saveOptions.fileName = argv[i];
            }
        } else if (std::string("-m") == argv[i]) {
            if (++i < argc) {
                saveOptions.maskFileName = argv[i];
                saveOptions.saveMask = true;
            }
        } else if (std::string("-v") == argv[i]) {
            Log::setMinimumPriority(1);
        } else if (std::string("-vv") == argv[i]) {
            Log::setMinimumPriority(0);
        } else if (std::string("--no-align") == argv[i]) {
            generalOptions.align = false;
        } else if (std::string("--no-crop") == argv[i]) {
            generalOptions.crop = false;
        } else if (std::string("--batch") == argv[i] || std::string("-B") == argv[i]) {
            generalOptions.batch = true;
        } else if (std::string("--single") == argv[i]) {
            generalOptions.withSingles = true;
        } else if (std::string("--help") == argv[i]) {
            help = true;
        } else if (std::string("-b") == argv[i]) {
            if (++i < argc) {
                try {
                    int value = std::stoi(argv[i]);
                    if (value == 32 || value == 24 || value == 16) saveOptions.bps = value;
                } catch (std::invalid_argument & e) {
                    std::cerr << tr("Invalid %1 parameter, using default.").arg(argv[i - 1]) << std::endl;
                }
            }
        } else if (std::string("-w") == argv[i]) {
            if (++i < argc) {
                try {
                    generalOptions.customWl = std::stoi(argv[i]);
                    generalOptions.useCustomWl = true;
                } catch (std::invalid_argument & e) {
                    std::cerr << tr("Invalid %1 parameter, using default.").arg(argv[i - 1]) << std::endl;
                    generalOptions.useCustomWl = false;
                }
            }
        } else if (std::string("-g") == argv[i]) {
            if (++i < argc) {
                try {
                    generalOptions.batchGap = std::stod(argv[i]);
                } catch (std::invalid_argument & e) {
                    std::cerr << tr("Invalid %1 parameter, using default.").arg(argv[i - 1]) << std::endl;
                }
            }
        } else if (std::string("-r") == argv[i]) {
            if (++i < argc) {
                try {
                    saveOptions.featherRadius = std::stoi(argv[i]);
                } catch (std::invalid_argument & e) {
                    std::cerr << tr("Invalid %1 parameter, using default.").arg(argv[i - 1]) << std::endl;
                }
            }
        } else if (std::string("-p") == argv[i]) {
            if (++i < argc) {
                std::string previewWidth(argv[i]);
                if (previewWidth == "full") {
                    saveOptions.previewSize = 2;
                } else if (previewWidth == "half") {
                    saveOptions.previewSize = 1;
                } else if (previewWidth == "none") {
                    saveOptions.previewSize = 0;
                } else {
                    std::cerr << tr("Invalid %1 parameter, using default.").arg(argv[i - 1]) << std::endl;
                }
            }
        } else if (argv[i][0] != '-') {
            generalOptions.fileNames.push_back(QString::fromLocal8Bit(argv[i]));
        }
    }
}


void Launcher::showHelp() {
    auto tr = [&] (const char * text) { return QCoreApplication::translate("Help", text); };
    std::cout << tr("Usage") << ": HDRMerge [--help] [OPTIONS ...] [RAW_FILES ...]" << std::endl;
    std::cout << tr("Merges RAW_FILES into an HDR DNG raw image.") << std::endl;
#ifndef NO_GUI
    std::cout << tr("If neither -a nor -o, nor --batch options are given, the GUI will be presented.") << std::endl;
#endif
    std::cout << tr("If similar options are specified, only the last one prevails.") << std::endl;
    std::cout << std::endl;
    std::cout << tr("Options:") << std::endl;
    std::cout << "    " << "--help        " << tr("Shows this message.") << std::endl;
    std::cout << "    " << "-o OUT_FILE   " << tr("Sets OUT_FILE as the output file name.") << std::endl;
    std::cout << "    " << "              " << tr("The following parameters are accepted, most useful in batch mode:") << std::endl;
    std::cout << "    " << "              - %if[n]: " << tr("Replaced by the base file name of image n. Image file names") << std::endl;
    std::cout << "    " << "                " << tr("are first sorted in lexicographical order. Besides, n = -1 is the") << std::endl;
    std::cout << "    " << "                " << tr("last image, n = -2 is the previous to the last image, and so on.") << std::endl;
    std::cout << "    " << "              - %iF[n]: " << tr("Replaced by the base file name of image n without the extension.") << std::endl;
    std::cout << "    " << "              - %id[n]: " << tr("Replaced by the directory name of image n.") << std::endl;
    std::cout << "    " << "              - %in[n]: " << tr("Replaced by the numerical suffix of image n, if it exists.") << std::endl;
    std::cout << "    " << "                " << tr("For instance, in IMG_1234.CR2, the numerical suffix would be 1234.") << std::endl;
    std::cout << "    " << "              - %%: " << tr("Replaced by a single %.") << std::endl;
    std::cout << "    " << "-a            " << tr("Calculates the output file name as") << " %id[-1]/%iF[0]-%in[-1].dng." << std::endl;
    std::cout << "    " << "-B|--batch    " << tr("Batch mode: Input images are automatically grouped into bracketed sets,") << std::endl;
    std::cout << "    " << "              " << tr("by comparing the creation time. Implies -a if no output file name is given.") << std::endl;
    std::cout << "    " << "-g gap        " << tr("Batch gap, maximum difference in seconds between two images of the same set.") << std::endl;
    std::cout << "    " << "--single      " << tr("Include single images in batch mode (the default is to skip them.)") << std::endl;
    std::cout << "    " << "-b BPS        " << tr("Bits per sample, can be 16, 24 or 32.") << std::endl;
    std::cout << "    " << "--no-align    " << tr("Do not auto-align source images.") << std::endl;
    std::cout << "    " << "--no-crop     " << tr("Do not crop the output image to the optimum size.") << std::endl;
    std::cout << "    " << "-m MASK_FILE  " << tr("Saves the mask to MASK_FILE as a PNG image.") << std::endl;
    std::cout << "    " << "              " << tr("Besides the parameters accepted by -o, it also accepts:") << std::endl;
    std::cout << "    " << "              - %of: " << tr("Replaced by the base file name of the output file.") << std::endl;
    std::cout << "    " << "              - %od: " << tr("Replaced by the directory name of the output file.") << std::endl;
    std::cout << "    " << "-r radius     " << tr("Mask blur radius, to soften transitions between images. Default is 3 pixels.") << std::endl;
    std::cout << "    " << "-p size       " << tr("Preview size. Can be full, half or none.") << std::endl;
    std::cout << "    " << "-v            " << tr("Verbose mode.") << std::endl;
    std::cout << "    " << "-vv           " << tr("Debug mode.") << std::endl;
    std::cout << "    " << "-w whitelevel " << tr("Use custom white level.") << std::endl;
    std::cout << "    " << "RAW_FILES     " << tr("The input raw files.") << std::endl;
}


bool Launcher::checkGUI() {
    int numFiles = 0;
    bool useGUI = true;
    for (int i = 1; i < argc; ++i) {
        if (std::string("-o") == argv[i]) {
            if (++i < argc) {
                useGUI = false;
            }
        } else if (std::string("-a") == argv[i]) {
            useGUI = false;
        } else if (std::string("--batch") == argv[i]) {
            useGUI = false;
        } else if (std::string("-B") == argv[i]) {
            useGUI = false;
        } else if (std::string("--help") == argv[i]) {
            return false;
        } else if (argv[i][0] != '-') {
            numFiles++;
        }
    }
    return useGUI || numFiles == 0;
}


int Launcher::run() {
#ifndef NO_GUI
    bool useGUI = checkGUI();
#else
    bool useGUI = false;
    help = checkGUI();
#endif
    QApplication app(argc, argv, useGUI);

    // Settings
    QCoreApplication::setOrganizationName("J.Celaya");
    QCoreApplication::setApplicationName("HdrMerge");

    // Translation
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale::system(), "qt", "_",
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    QTranslator appTranslator;
    if (appTranslator.load(QLocale::system(), "hdrmerge", "_", ":/translators")) {
        app.installTranslator(&appTranslator);
    }

    parseCommandLine();
    Log::debug("Using LibRaw ", libraw_version());

    if (help) {
        showHelp();
        return 0;
    } else if (useGUI) {
        return startGUI();
    } else {
        return automaticMerge();
    }
}

} // namespace hdrmerge
