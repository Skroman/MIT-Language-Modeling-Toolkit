////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008, Massachusetts Institute of Technology              //
// All rights reserved.                                                   //
//                                                                        //
// Redistribution and use in source and binary forms, with or without     //
// modification, are permitted provided that the following conditions are //
// met:                                                                   //
//                                                                        //
//     * Redistributions of source code must retain the above copyright   //
//       notice, this list of conditions and the following disclaimer.    //
//                                                                        //
//     * Redistributions in binary form must reproduce the above          //
//       copyright notice, this list of conditions and the following      //
//       disclaimer in the documentation and/or other materials provided  //
//       with the distribution.                                           //
//                                                                        //
//     * Neither the name of the Massachusetts Institute of Technology    //
//       nor the names of its contributors may be used to endorse or      //
//       promote products derived from this software without specific     //
//       prior written permission.                                        //
//                                                                        //
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS    //
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT      //
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR  //
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT   //
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  //
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT       //
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,  //
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY  //
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT    //
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  //
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.   //
////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <boost/program_options.hpp>
#include "util/Logger.h"
#include "util/ZFile.h"
#include "Types.h"
#include "NgramPerplexity.h"

using std::vector;
using std::string;
using std::cout;
namespace po = boost::program_options;
namespace cls = boost::program_options::command_line_style;

////////////////////////////////////////////////////////////////////////////////

const char *version = "0.1";
const char *progDesc =
"Usage: evaluate-ngram [Options]\n"
"\n"
"evaluates performance of an n-gram language model.  It also supports various\n"
"n-gram language model conversions, including changes in order, vocabulary,\n"
"and file format.\n"
"\n"
"Filename argument can be an ASCII file, a compressed file (ending in .Z or .gz),\n"
"or '-' to indicate stdin/stdout.\n";

const char *footerDesc =
"Examples:\n"
"\n"
"Compute the perplexity of a n-gram LM on a text file.\n"
"   evaluate-ngram -read-lm input.lm -evaluate-perplexity test.txt\n"
"\n"
"Convert a trigram model to a bigram LM.\n"
"   evaluate-ngram -read-lm input.lm -order 2 -write-lm output.lm\n"
"\n"
"Convert a binary n-gram LM to ARPA text format.\n"
"   evaluate-ngram -read-lm input.blm -write-lm output.lm\n";

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
    // Parse command line options.
    int order, verbosity;
    po::options_description desc(string("Options"));
    desc.add_options()
        ("help", "Print this message.")
        ("version", "Print version number.")
        ("verbose", po::value<int>(&verbosity)->default_value(1),
         "Set verbosity level.")
        ("order,o", po::value<int>(&order)->default_value(3),
         "Set the n-gram order of the estimated LM.")
        ("read-vocab,v", po::value<string>(),
         "Restrict the vocabulary to only words from the specified vocabulary "
         "vocabfile.  n-grams with out of vocabulary words are ignored.")
        ("read-lm,l", po::value<string>(),
         "Reads n-gram language model in either ARPA or binary format.")
        ("evaluate-perplexity,e", po::value<vector<string> >()->composing(),
         "Compute the perplexity of textfile.  This option can be repeated.")
        ("write-vocab,V", po::value<string>(),
         "Write the LM vocabulary to vocabfile.")
        ("write-lm,L", po::value<string>(),
         "Write the interpolated n-gram language model to lmfile in ARPA "
         "backoff text format.")
        ("write-binary-lm,B", po::value<string>(),
         "Write  the interpolated n-gram language model to lmfile in MITLM "
         "binary format.")
        ;

    po::positional_options_description pd;
    pd.add("read-lm", -1);

    // Allow typical UNIX command line styles, except sticky (-s -k become -sk).
    // Add support for single dash long options (-help).
    cls::style_t options_style = (cls::style_t)(cls::allow_dash_for_short |
        cls::allow_short | cls::short_allow_adjacent | cls::short_allow_next |
        cls::allow_long | cls::long_allow_adjacent | cls::long_allow_next |
        cls::allow_long_disguise);
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc)
              .style(options_style).positional(pd).run(), vm);
    //po::store(parse_config_file("example.cfg", desc), vm);
    po::notify(vm);

    // Process basic command line arguments.
    Logger::SetVerbosity(verbosity);
    if (vm.count("version")) {
        cout << "MIT Language Modeling Toolkit (v" << version << ")\n"
             << "Copyright (C) 2008 Bo-June (Paul) Hsu\n"
             << "MIT Computer Science and Artificial Intelligence Laboratory\n";
        return 0;
    }
    if (vm.count("help") || vm.count("read-lm") == 0) {
        cout << progDesc << "\n" << desc << "\n" << footerDesc << "\n";
        return 1;
    }

    ArpaNgramLM lm(order);

    if (vm.count("read-vocab")) {
        const char *vocabFile = vm["read-vocab"].as<string>().c_str();
        Logger::Log(1, "Loading vocab %s...\n", vocabFile);
        lm.LoadVocab(ZFile(vocabFile));
    }

    // Read language model.
    const char *readLM = vm["read-lm"].as<string>().c_str();
    Logger::Log(1, "Loading LM %s...\n", readLM);
    lm.LoadLM(ZFile(readLM, "r"));

    // Evaluate LM.
    if (vm.count("evaluate-perplexity")) {
        const vector<string> &evalFiles = vm["evaluate-perplexity"]
            .as<vector<string> >();

        Logger::Log(0, "Perplexity Evaluations:\n");
        for (size_t i = 0; i < evalFiles.size(); i++) {
            Logger::Log(1, "Loading eval set %s...\n", evalFiles[i].c_str());
            NgramPerplexity eval(lm);
            eval.LoadCorpus(ZFile(evalFiles[i].c_str()));

            Logger::Log(0, "\t%s\t%.3f\n", evalFiles[i].c_str(),
                       eval.ComputePerplexity(lm.defParams()));
        }
    }

    // Save results.
    if (vm.count("write-vocab")) {
        const char *vocabFile = vm["write-vocab"].as<string>().c_str();
        Logger::Log(1, "Saving vocabulary to %s...\n", vocabFile);
        lm.SaveVocab(ZFile(vocabFile, "w"));
    }
    if (vm.count("write-lm")) {
        const char *lmFile = vm["write-lm"].as<string>().c_str();
        Logger::Log(1, "Saving LM to %s...\n", lmFile);
        lm.SaveLM(ZFile(lmFile, "w"));
    }
    if (vm.count("write-binary-lm")) {
        const char *lmFile = vm["write-binary-lm"].as<string>().c_str();
        Logger::Log(1, "Saving binary LM to %s...\n", lmFile);
        lm.SaveLM(ZFile(lmFile, "wb"), true);
    }

    return 0;
}
