////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2011, Abram Hindle, Prem Devanbu and UC Davis            //
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

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
//#include <boost/pending/fibonacci_heap.hpp>
//#include <boost/pending/relaxed_heap.hpp>
#include <algorithm>

#include "util/FastIO.h"
#include "util/ZFile.h"
#include "util/FakeZFile.h"
#include "util/CommandOptions.h"
#include "Types.h"
#include "NgramModel.h"
#include "NgramLM.h"
#include "Smoothing.h"
#include "CrossFolder.h"
#include <cstdlib>
#include "util/Logger.h"

using std::string;
using std::vector;
using std::stringstream;


#include "LiveGuess.h"

typedef std::less<VocabProb> VCompare;
//typedef boost::typed_identity_property_map<long unsigned int> VID;


// double forwardish(double * forward, /* vocab.size() * numobs */
//                   const int numobs, 
//                   const NgramVector & ngrams,
//                   const ProbVector & probabilities) {
//   int numstates = vocab.size();
//   int nn =  numstates * numobs;
//   for (int i = 0; i < nn; i++) {
//     forward[i] = 0.0;
//   }
//   forward[0] = 1.0;
//   for (int i = 1; i < numobs; i++) {
//     for (int s = 1; i < numstates; s++) {
//       double sum = 0.0;
//       for (int p = 0; p < numstates; p++) { // previous
//         sum += forward[ ] * 
//       }
//     }
//   }
// }
//   

void mkHeap(vector<VocabProb> & heap) {
  // I hate the STL heaps
  make_heap (heap.begin(),heap.end());  
}
void popHeap( vector<VocabProb> & heap ) {
  pop_heap (heap.begin(),heap.end());  
  heap.pop_back();
}
void pushHeap( vector<VocabProb> & heap, VocabProb & v) {
  heap.push_back( v ); 
  push_heap( heap.begin(), heap.end() );
}
void sortHeap( vector<VocabProb> & heap ) { 
  sort_heap( heap.begin(), heap.end() ); // heap now is internally sorted 
}

// WARNING THIS ALLOCATES MEMORY //
char * joinVectorOfCStrings( vector<const char*> & words ) {
  int len = 1; // null term
  for (int i = 0; i < words.size(); i++) {
    len += strlen( words[i] );
  }
  if (words.size() > 0) {
    len += words.size() - 1;
  }
  char * v = new char[ len ];
  char * o = v;
  for (int i = 0; i < words.size(); i++) {
    char * word = words[i];
    int wlen = strlen( word );
    CopyString( o, word );
    o += wlen; // jump to end of word
    if (i < words.size() - 1) {
      *o = ' '; //add space
    }
    o++; // skip space
  }
  return v;
}

NgramIndex findIndex( 
                     const vector<const char *> & words,
                     const NgramLMBase & _lm, 
                     const int _order,  
                     const Vocab & vocab ) {

  NgramIndex index = 0;
  for (int i = 0; i < _order - 1; ++i) {
    const ProbVector & probabilities = _lm.probs( i  );
    const NgramVector & ngrams = _lm.model().vectors( i + 1   );
    const char * sWord = words[ words.size() - (_order - 1) + i ];
    VocabIndex vWordI = vocab.Find( sWord );
    index = ngrams.Find(index, vWordI);
    Prob prob = probabilities[ index ];
    Logger::Log(0, "Word:\t%d\t%s\t%d\t\%d\t%e\n", i, sWord, vWordI, index, prob);    
  }
  return index;
}



// Returns a vector of LiveGuessResults
// warning: words is mutated temporarily
std::auto_ptr< std::vector<LiveGuessResult> > 
forwardish(Vector<const char *> & words, // the current words can be empty
           const double currentProb,
           const int size, // how many to grab
           const int depthLeft,
           const NgramIndex index,
           const NgramLMBase & _lm, 
           const int _order,  
           const Vocab & vocab ) {
  
  
  // Index contains the last ngram word 

  vector<VocabProb> heap(0);

  mkHeap(heap);
  //boost::fibonacci_heap<VocabProb> heap( size, cmp );    
  const NgramVector & ngrams = _lm.model().vectors( _order );
  const ProbVector & probabilities = _lm.probs( _order - 1  );
  int count = 0;
  for (int j = 0; j < vocab.size(); j++) {
    NgramIndex newIndex = ngrams.Find( index, j);
    Prob prob = probabilities[ newIndex ];
    const VocabProb v( -1 * prob,j, newIndex);
    if ( count < size ) {
      heap.push_back( v ); //push_heap( heap.begin(), heap.end() );
      count++;
      if (count == size) {
        mkHeap( heap );
      }
      // this is irritating, basically it means the highest rank stuff
      // will be in the list and we only kick out the lowest ranked stuff
      // (which will be the GREATEST of what is already there)
    } else if ( -1 * heap.front().prob < prob ) {
      // this is dumb        
      // remove the least element
      popHeap( heap );
      pushHeap( heap, v );
      // should we update?
    }
  }
  sortHeap( heap );

  std::vector<LiveGuessResult> & resVector = new std::vector<LiveGuessResult>();
  for( int j = 0; j < heap.size(); j++) {
    VocabProb v = heap[ j ];
    Prob prob = -1 * v.prob;
    prob += currentProb;
    words.push_back( vocab[ v.index ] ); // add 
    resVector.push_back( LiveGuessResult( prob , joinVectorOfCStrings( words ) , true)); // dont copy cuz it is already allocated
    word.pop_back(); // and restore
  }
  
  if ( depthLeft == 0 ) {
    // return what we have
  } else {
  for( int j = 0; j < heap.size(); j++) {
    VocabProb v = heap[ j ];
    Prob prob = -1 * v.prob;
    NgramIndex nindex = v.nindex;
    prob += currentProb;
    words.push_back( vocab[ v.index ] );
    std::auto_ptr< std::vector<LiveGuessResult> > r = 
      forwardish( words, 
                  prob,
                  size,
                  depthLeft - 1,
                  nindex,
                  _lm, 
                  _order,  
                  vocab );
    word.pop_back(); // and restore
    for (int i = 0; i < r.size(); i++) {
      resVector.push_back( r[i] );
    }
  }


  std::auto_ptr< std::vector<LiveGuessResult> > returnValues( resVector );
  

}

std::auto_ptr< std::vector<LiveGuessResult> > LiveGuess::Predict( char * str, int predictions ) {
  vector<const char *> words(0);
  int len = strlen(str) + 1;
  char strSpace[len];
  strcpy( strSpace, str);
  char * p = strSpace;
  // parse words
  while (*p != '\0') {
    while (isspace(*p)) ++p;  // Skip consecutive spaces.
    const char *token = p;
    while (*p != 0 && !isspace(*p))  ++p;
    size_t len = p - token;
    if (*p != 0) *p++ = 0;
    words.push_back( token );
  }
  Logger::Log(0, "We've got these words:\n");
  for (int i = 0 ; i < words.size(); i ++) {
    Logger::Log(0, "\tWord: %d - %s\n",i, words[i]);
  }
  // words are parsed
  const Vocab & vocab = _lm.vocab();
  for (int i = 0 ; i < words.size() - _order + 1; i++ ) {

    // Now we find the index of the n-gram
    NgramIndex index = 0;
    Prob prob = 0;
    const char * sWord = NULL;
    for (size_t j = 0; j < _order; ++j) {
      const ProbVector & probabilities = _lm.probs( j  );
      const NgramVector & ngrams = _lm.model().vectors( j + 1   );
      sWord = words[i + j];
      VocabIndex vWordI = vocab.Find( sWord );
      index = ngrams.Find(index, vWordI);
      prob = probabilities[ index ];
      //Logger::Log(0, "Word:\t%d\t%s\t%d\t\%d\t%e\n", j, sWord, vWordI, index, prob);
    }
    Logger::Log(0, "Final:\t%d\t%e\t%s\n", index, prob, sWord);

  }

  // now find the index of the last word if it was order - 1 ?
  NgramIndex index = 0;
  for (int i = 0; i < _order - 1; ++i) {
    const ProbVector & probabilities = _lm.probs( i  );
    const NgramVector & ngrams = _lm.model().vectors( i + 1   );
    const char * sWord = words[ words.size() - (_order - 1) + i ];
    VocabIndex vWordI = vocab.Find( sWord );
    index = ngrams.Find(index, vWordI);
    Prob prob = probabilities[ index ];
    Logger::Log(0, "Word:\t%d\t%s\t%d\t\%d\t%e\n", i, sWord, vWordI, index, prob);    
  }
  double probpred[predictions];
  const int size = 10;
  // const VCompare cmp();
  // const boost::identity_property_map ID();
  //const VID id();
  // const boost::typed_identity_property_map<long unsigned int> ID();
  // boost::fibonacci_heap<VocabProb, std::less<VocabProb>, boost::identity_property_map >::fibonacci_heap(size , cmp, ID );
  // boost::fibonacci_heap<VocabProb, std::less<VocabProb>, boost::typed_identity_property_map<long unsigned int> >::fibonacci_heap heap( size, cmp, id);
  //boost::fibonacci_heap<VocabProb, VCompare>::fibonacci_heap heap( size, VCompare());
  //boost::relaxed_heap<VocabProb, VCompare>::relaxed_heap heap( (const long unsigned int)10 );//, VCompare());
  vector<VocabProb> heap(0);
  // I hate the STL heaps
  make_heap (heap.begin(),heap.end());
  //boost::fibonacci_heap<VocabProb> heap( size, cmp );    
  const NgramVector & ngrams = _lm.model().vectors( _order );
  const ProbVector & probabilities = _lm.probs( _order - 1  );
  int count = 0;
  for (int j = 0; j < vocab.size(); j++) {
    NgramIndex newIndex = ngrams.Find( index, j);
    Prob prob = probabilities[ newIndex ];
    const VocabProb v( -1 * prob,j);
    if ( count < size ) {
      heap.push_back( v ); //push_heap( heap.begin(), heap.end() );
      count++;
      if (count == size) {
          make_heap (heap.begin(),heap.end());
      }
      // this is irritating, basically it means the highest rank stuff
      // will be in the list and we only kick out the lowest ranked stuff
      // (which will be the GREATEST of what is already there)
    } else if ( -1 * heap.front().prob < prob ) {
      // this is dumb        
      pop_heap (heap.begin(),heap.end());  heap.pop_back();
      heap.push_back( v ); push_heap( heap.begin(), heap.end() );
      // should we update?
    }
  }
  sort_heap( heap.begin(), heap.end() ); // heap now is internally sorted 
  for( int j = 0; j < heap.size(); j++) {
    VocabProb v = heap[ j ];
    Logger::Log(0, "%d\t%e\t%s\n", j, log( -1 * v.prob ) / log(10), vocab[ v.index ]);
  }

  // left to do:
  // - iterate over a few possibilities and chains
  // - return those results


  std::auto_ptr< std::vector<LiveGuessResult> > returnValue( new std::vector<LiveGuessResult>() );
  return returnValue;
}
