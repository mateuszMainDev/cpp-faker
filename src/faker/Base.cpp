#include <iostream>
#include <locale>
#include <chrono>

#include "Base.h"
#include "Data.h"
#include "StringVector.h"

using std::cout;
using std::endl;
using std::string;
using namespace Faker;

string Base::configDirName = "/usr/local/etc/cpp-faker/locales";
string Base::locale;			// en_US
string Base::localeBase;		// en
std::mutex Base::baseMutex;
std::mt19937 Base::randomGenerator;
bool Base::haveBeenSetup = false;
bool Base::haveBeenSeeded = false;

/**
 * Seed the random number generator with a specific value.
 */
void
Base::seed(unsigned long val) {
    randomGenerator.seed(val);
    haveBeenSeeded = true;
}

/**
 * Make sure we've been configured.
 *
 * This code may not really work. I've never used std::locale before and the example I'm starting
 * with is stupid. On my Mac, I'm getting a default locale of "C". Yuck.
 */
void
Base::setup() {
    std::unique_lock<std::mutex> mlock(baseMutex);
    if (!haveBeenSetup) {
        haveBeenSetup = true;
        locale = std::locale().name();

        if (locale == "C") {
            std::locale::global(std::locale("en_US"));
            locale = std::locale().name();
            localeBase = locale.substr(0, locale.find("_"));
        }

        if (!haveBeenSeeded) {
            randomGenerator.seed(std::chrono::system_clock::now().time_since_epoch().count());
            haveBeenSeeded = true;
        }
    }
}

/**
 * Return a random number in the inclusive range low..high.
 */
long
Base::randomNumber(long low, long high) {
    long retVal = 0;
    long range = high - low + 1;	// For 0..5, we have 6 possible values.

    setup();

    retVal = (randomGenerator() % range) + low;

    return retVal;
}

/**
 * Parse the string. This is the core of the entire library. From Name, we see things
 * like:
 *
 * 		name.name
 * 		name.first_name
 *
 * First pass, we look up from our JSON configuration:
 *
 * 		Load localBase and locale (en and en_US).
 * 		Under that, look for en.faker.name
 * 		Under that, look for en.faker.name.name (or first_name, or whatever)
 */
std::string
Base::parse(const string &path, const std::string &beginningLocale, const std::string &domain) {
    setup();

    string ourLocale = beginningLocale != "" ? beginningLocale : localeBase;

    StringVector parts;
    parts.tokenize(path, ".");

    Data::Vector dataStack;
    Data::Pointer localeData = Data::globalData.find(ourLocale);

    // This is probably something like "en".
    if (localeData == nullptr) {
        Data::globalData.load(configDirName, ourLocale);
        localeData = Data::globalData.find(ourLocale);
        if (localeData == nullptr) {
            std::cerr << "Faker: locale " << ourLocale << " not found" << endl;
            return "BadValue";
        }

        dataStack.push_back(localeData);
    }

    // This is probably something like "faker".
    Data::Pointer domainData = localeData->find(domain);
    if (domainData == nullptr) {
        domainData = localeData->makeDomain(domain);
        dataStack.push_back(domainData);
    }

    Data::Pointer currentPointer = nullptr;
    if (parts.size() > 0) {
        string topName = parts.at(0);
        Data::Pointer topFound = domainData->find(topName);

        if (topFound == nullptr) {
            Data::globalData.load(configDirName + "/" + ourLocale, topName);
            //cout << endl << endl << "We loaded: " << endl << endl;
            //Data::globalData.dumpTree(3, "Loaded: ");

            //cout << endl << endl;

            topFound = domainData->find(topName);
            dataStack.push_back(domainData);
        }
        if (topFound == nullptr) {
            cout << "Can't load: " << topName << endl;
        }
        else {
            dataStack.push_back(topFound);

            string pathSoFar = topName;
            bool atTop = true;

            currentPointer = topFound;
            for (string str: parts) {
                if (atTop) {
                    atTop = false;
                }
                else {
                    pathSoFar += "." + str;
                    Data::Pointer found = currentPointer->find(str);

                    if (found == nullptr) {
                        cout << "Can't find: " << path << endl;
                        break;
                    }
                    dataStack.push_back(found);
                    currentPointer = found;
                }
            }
        }
    }

    //======================================================================
    // The above gets the data loaded as best we can. Now we've narrowed
    // it down to something to pick from.
    //======================================================================
    if (currentPointer != nullptr) {
        //cout << endl << endl;
        //currentPointer->dumpTree(1, path + ": ");
        return currentPointer->expand(dataStack);
    }

    return "";
}

/**
 * Produces a random string of digits.
 */
std::string Faker::Base::randomDigits(int length) {
    string retVal = "";

    for (int index = 0; index < length; ++index) {
        char c = '0' +  randomNumber(0, 9);
        retVal += c;
    }

    return retVal;
}
