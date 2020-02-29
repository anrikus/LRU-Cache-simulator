#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>
#include <bits/stdc++.h>
#include <algorithm>
#include <vector>

using namespace std;

string::size_type sz;
long opCount = 1;
long filePointer = 0;

// Conversion of hex adddress to tag and index
struct address{
    string blockAdd;
    string tag;
    int index;
};

address addMaker(string hexAdd, int blockSize, int numSets){
    long decAdd = stol(hexAdd, &sz, 16);
    int blockBits = ceil(log2(blockSize));
    int setBits = ceil(log2(numSets));
    long blockAdd  = decAdd % long(pow(2, blockBits));
    blockAdd = decAdd - blockAdd;
    long tagAdd = decAdd / long(pow(2, setBits+blockBits));
    int index = (decAdd % long(pow(2, setBits+blockBits))) / long(pow(2, blockBits));
    address newAdd;
    std::stringstream sstream;
    sstream << std::hex << tagAdd;
    newAdd.tag = sstream.str();
    std::stringstream sstream2;
    sstream2 << std::hex << blockAdd;
    newAdd.blockAdd =  sstream2.str();
    newAdd.index =  index;
    return(newAdd);
}


//Initial Parameters
long blockSize;
long l1Size;
long l1Assoc;
long l2Size;
long l2Assoc;
long rPolicy;
long iPolicy;
string traceFile;

//Performance parameters
long l1Reads = 0;
long l1ReadMisses = 0;
long l1Writes = 0;
long l1WriteMisses = 0;
long l1MissRate = 0;
long l1Writebacks = 0;
long l2Reads = 0;
long l2ReadMisses = 0;
long l2Writes = 0;
long l2WriteMisses = 0;
long l2MissRate = 0;
long l2WriteBacks = 0;
long totalMemoryTraffic = 0;
long l1DirectWriteBacks = 0;

//Block parameters
struct block
{
    string blockAdd;
    string hexAdd;
    string tag;
    int index;
    bool valid;
    long serial;
    long dirtyBit;
};

//Serial Number counters
long l1Serial = 1;
long l2Serial = 1;

//L1 Cache identifier
long l1Sets;
block** l1;

//L2 Cache identifier
long l2Sets;
block**l2;

// This structure will form base for the input content.
struct fileContentStruct{
    string ops;
    string hexAdd;
};

// THis is vector of trace file lines. Each line is one object.
// global vector
vector<fileContentStruct*> traceFileContent;

//creating new struct object out of each line in trace.
//Memory will be allocated in heap.
//globally accessible.
fileContentStruct* getFileContentStruct(string ops, string addr)
{
    fileContentStruct *temp = new fileContentStruct();
    temp->hexAdd = addr;
    temp->ops = ops;
    return temp;
}

// Read Whole trace file and make vector out of it.
void readTraceFile(string traceFileName)
{ 
    ifstream infile(traceFileName);
    string op;
    string hexAdd;
    while(infile >> op >> hexAdd )
    {
        traceFileContent.push_back(getFileContentStruct(op,hexAdd));
    }
}

void getBlocksToDeleteForOptimalPolicy(vector<fileContentStruct*> &traceFileContent, int __startIndex, vector<string> &__set, int blockSize, int numSets)
{   
   for(int i =__startIndex ; i<traceFileContent.size();i++)
   {
       if(__set.size() == 1)
            return;
        for(int j = 0;j<__set.size();j++)
        {
            address newAdd = addMaker(traceFileContent[i]->hexAdd, blockSize, numSets);
            if(__set[j] == newAdd.blockAdd) //
            {
                __set.erase(__set.begin()+j);
                break;
            }
        }
   }
   return;
}


//l1 Invalidate
bool l1Invalidate(string hexAdd){
    address newAdd = addMaker(hexAdd, blockSize, l1Sets);
    for (int j=0; j<l1Assoc; j++){
        if (l1[newAdd.index][j].blockAdd==newAdd.blockAdd){
            if (l1[newAdd.index][j].valid == true){
                l1[newAdd.index][j].valid = false;
                if (l1[newAdd.index][j].dirtyBit == 1){
                    //cout<<"L1 invalidated: "<<newAdd.blockAdd<<" (tag "<<newAdd.tag<<", index "<<newAdd.index<<", dirty)"<<endl;
                    //cout<<"L1 writeback to main memory directly"<<endl;
                    //l1Writebacks += 1;
                    l1DirectWriteBacks += 1;
                    return(true);
                }
                if (l1[newAdd.index][j].dirtyBit == 0){
                    //cout<<"L1 invalidated: "<<newAdd.blockAdd<<" (tag "<<newAdd.tag<<", index "<<newAdd.index<<", clean)"<<endl;
                    return(false);
                }
            }
        }
    }
    return(false);
}

//L2 write
void l2Write(string hexAdd, int write){
    if (l2Size==0){
        return;
    }
    
    //Get the tag and index
    address newAdd = addMaker(hexAdd, blockSize, l2Sets);

    if (write==1){
        //cout<<"L2 write : "<<newAdd.blockAdd<<" (tag "<<newAdd.tag<<", index "<<newAdd.index<<")"<<endl;
        l2Writes += 1;
    }

    //If explicit write
    if (write==1){
        //Check if the address is available, then set the dirty bit to true and return
        for (int i=0; i<l2Assoc; i++){
            if (l2[newAdd.index][i].tag == newAdd.tag && l2[newAdd.index][i].valid==true){
                //cout<<"L2 hit"<<endl;
                //LRU
                if (rPolicy==0)
                {
                    //cout<<"L2 update LRU"<<endl;
                    l2[newAdd.index][i].serial = l2Serial++;
                }
                if (rPolicy==1){
                //cout<<"L2 update FIFO"<<endl;
                }
                if (rPolicy==2){
                //cout<<"L2 update optimal"<<endl;
                l2[newAdd.index][i].serial = l2Serial++;
                }
                //cout<<"L2 set dirty"<<endl;
                l2[newAdd.index][i].dirtyBit = 1;
                return;
            }
        }
        l2WriteMisses += 1;
        //cout<<"L2 miss"<<endl;
    }
    //Else:
    //Determine the victim (empty slot or the minimum serial)
    long victimSerial = INT_MAX;
    
    if (rPolicy==0 || rPolicy==1){
        for (int i=0; i<l2Assoc; i++){
            if (l2[newAdd.index][i].valid==false){
                victimSerial = l2[newAdd.index][i].serial;
                break;
            }
            else {
                victimSerial = min(long(victimSerial), l2[newAdd.index][i].serial);
            }
        }
    }

    if (rPolicy==2){
        //Check if an invalid slot is available
        for (int j=0; j<l1Assoc; j++){
            if (l2[newAdd.index][j].valid==false){
                victimSerial = l2[newAdd.index][j].serial;
                break;
            }
        }
        //Determine optimal replacement in case no invalid slots are avialable
        if (victimSerial==INT_MAX){
            vector<string> set;
            for(int j=0; j<l2Assoc; j++){
                set.push_back(l2[newAdd.index][j].blockAdd);
            }
            getBlocksToDeleteForOptimalPolicy(traceFileContent, filePointer + 1, set, blockSize, l2Sets);
            string victimBlockAdd= set[0];
            for (int j=0; j<l1Assoc; j++){
                if (l2[newAdd.index][j].blockAdd==victimBlockAdd){
                    victimSerial = l2[newAdd.index][j].serial;
                    break;
                }
            }
        }
    }



    //Insert in the slot
    for (int i=0; i<l2Assoc; i++){
        if (l2[newAdd.index][i].serial==victimSerial){
            //Check for valid and print status accordingly
            if (l2[newAdd.index][i].valid==true){
                if (l2[newAdd.index][i].dirtyBit==true){
                    //cout<<"L2 victim: "<<l2[newAdd.index][i].blockAdd<<" (tag "<<l2[newAdd.index][i].tag<<", index "<<l2[newAdd.index][i].index<<", dirty"<<")"<<endl;
                    
                    if (iPolicy==1){
                        bool r = l1Invalidate(l2[newAdd.index][i].hexAdd);
                        if (r==false){
                            l2WriteBacks += 1;
                        }
                    }

                    else {
                        l2WriteBacks += 1;
                    }
                }
                else{
                    //cout<<"L2 victim: "<<l2[newAdd.index][i].blockAdd<<" (tag "<<l2[newAdd.index][i].tag<<", index "<<l2[newAdd.index][i].index<<", clean"<<")"<<endl;
                    if (iPolicy==1){
                        l1Invalidate(l2[newAdd.index][i].hexAdd);
                    }
                }
            }
            else{
                //cout<<"L2 victim: none"<<endl;
            }
            l2[newAdd.index][i].blockAdd = newAdd.blockAdd;
            l2[newAdd.index][i].index = newAdd.index;
            l2[newAdd.index][i].hexAdd = hexAdd;
            l2[newAdd.index][i].tag = newAdd.tag;
            l2[newAdd.index][i].serial = l2Serial++;
            if (rPolicy==0){
                //cout<<"L2 update LRU"<<endl;
            }
            if (rPolicy==1){
                //cout<<"L2 update FIFO"<<endl;
            }
            if (rPolicy==2){
                //cout<<"L2 update optimal"<<endl;
            }

            l2[newAdd.index][i].valid = 1;
            // If write, then set dirty bit, else just read, so don't set dirty bit
            if (write==1){
                //cout<<"L2 set dirty"<<endl;
                l2[newAdd.index][i].dirtyBit = 1;
            }
            else {
                l2[newAdd.index][i].dirtyBit = 0;
            }
            break;
        }
    }
}



//L2 read function
void l2Read(string hexAdd){
    if (l2Size==0){
        return;
    }
    
    //Get the tag and index
    address newAdd = addMaker(hexAdd, blockSize, l2Sets);
    
    //Attempt L2 Read
    //cout<<"L2 read : "<<newAdd.blockAdd<<" "<<"(tag "<<newAdd.tag<<", "<<"index "<<newAdd.index<<")"<<endl; 
    l2Reads += 1;
    for (int i=0; i<l2Assoc; i++){
        if (l2[newAdd.index][i].tag == newAdd.tag && l2[newAdd.index][i].valid==true){
            //cout<<"L2 hit"<<endl;
            //LRU
            if (rPolicy==0){
                //cout<<"L2 update LRU"<<endl;
                l2[newAdd.index][i].serial = l2Serial++;
            }
            if (rPolicy==1){
                //cout<<"L1 update FIFO"<<endl;
            }
            if (rPolicy==2){
                //cout<<"L2 update optimal"<<endl;
                l2[newAdd.index][i].serial = l2Serial++;
            }
            return;
        }
    }
    //Not present in L2
    l2ReadMisses += 1;
    //cout<<"L2 miss"<<endl;

    //Fetch to L2 caches
    l2Write(hexAdd, 0);
}


// L1 write
void l1Write(string hexAdd, int write){
    
    //Get the tag and index
    address newAdd = addMaker(hexAdd, blockSize, l1Sets);

    //Print exlicit write status is so
    if (write==1){
        //cout<<"L1 write : "<<newAdd.blockAdd<<" "<<"(tag "<<newAdd.tag<<", "<<"index "<<newAdd.index<<")"<<endl;
    }

    //If explicit write
    if (write==1){
        l1Writes += 1;
        //Check if the address is available, then set the dirty bit to true and return
        for (int i=0; i<l1Assoc; i++){
            if (l1[newAdd.index][i].tag == newAdd.tag && l1[newAdd.index][i].valid==true){
                //cout<<"L1 hit"<<endl;
                //LRU
                if (rPolicy==0){
                    //cout<<"L1 update LRU"<<endl;
                    l1[newAdd.index][i].serial = l1Serial++;
                }
                if (rPolicy==1){
                    //cout<<"L1 update FIFO"<<endl;
                }
                if (rPolicy==2){
                    l1[newAdd.index][i].serial = l1Serial++;
                    //cout<<"L1 update optimal"<<endl;
                }
                //Set dirty bit
                l1[newAdd.index][i].dirtyBit = 1;
                //cout<<"L1 set dirty"<<endl;
                return;
            }
        }
        l1WriteMisses += 1;
        //l2Reads += 1;
        //cout<<"L1 miss"<<endl;
    }
    //Else:
    //Determine the victim (empty slot or the minimum serial)
    long victimSerial = INT_MAX;
    if (rPolicy==0 || rPolicy==1){
        for (int i=0; i<l1Assoc; i++){
            if (l1[newAdd.index][i].valid==false){
                victimSerial = l1[newAdd.index][i].serial;
                break;
            }
            else {
                victimSerial = min(victimSerial, l1[newAdd.index][i].serial);
            }
        }
    }

    if (rPolicy==2){
        //Check if an invalid slot is available
        for (int j=0; j<l1Assoc; j++){
            if (l1[newAdd.index][j].valid==false){
                victimSerial = l1[newAdd.index][j].serial;
                break;
            }
        }
        //Determine optimal replacement in case no invalid slots are avialable
        if (victimSerial==INT_MAX){
            vector<string> set;
            for(int j=0; j<l1Assoc; j++){
                set.push_back(l1[newAdd.index][j].blockAdd);
            }

            getBlocksToDeleteForOptimalPolicy(traceFileContent, filePointer + 1, set, blockSize, l1Sets);
            if (set.size()>1){
                //cout<<"Arbitrary"<<endl;
            }
            string victimBlockAdd = set[0];
            for (int j=0; j<l1Assoc; j++){
                if (l1[newAdd.index][j].blockAdd==victimBlockAdd){
                    victimSerial = l1[newAdd.index][j].serial;
                    break;
                }
            }
        }
    }
    
    //Insert in the slot
    for (int i=0; i<l1Assoc; i++){
        if (l1[newAdd.index][i].serial==victimSerial){
            //If empty slot
            if (l1[newAdd.index][i].valid==false){
                //cout<<"L1 victim: none"<<endl;
            }
            else{
                //if victim dirty bit is set, send write to L2
                if (l1[newAdd.index][i].dirtyBit==1){
                    //cout<<"L1 victim: "<<l1[newAdd.index][i].blockAdd<<" (tag "<<l1[newAdd.index][i].tag<<", index "<<l1[newAdd.index][i].index<<", dirty"<<")"<<endl;
                    l1[newAdd.index][i].valid = false;
                    l1Writebacks += 1;
                    l2Write(l1[newAdd.index][i].hexAdd, 1);   
                }
                else{
                    //cout<<"L1 victim: "<<l1[newAdd.index][i].blockAdd<<" (tag "<<l1[newAdd.index][i].tag<<", index "<<l1[newAdd.index][i].index<<", clean"<<")"<<endl;
                    l1[newAdd.index][i].valid = false;
                }
            }

            //Issue a read request to L2
            l2Read(hexAdd);
            
            l1[newAdd.index][i].valid = true;
            l1[newAdd.index][i].hexAdd = hexAdd;
            l1[newAdd.index][i].blockAdd = newAdd.blockAdd;
            l1[newAdd.index][i].tag = newAdd.tag;
            l1[newAdd.index][i].index = newAdd.index;
            l1[newAdd.index][i].serial = l1Serial++;
            if (rPolicy==0){
                //cout<<"L1 update LRU"<<endl;
            }
            if (rPolicy==1){
                //cout<<"L1 update FIFO"<<endl;
            }
            if (rPolicy==2){
                //cout<<"L1 update optimal"<<endl;
            }
            // If write, then set dirty bit, else just read, so don't set dirty bit
            if (write==1){
                //cout<<"L1 set dirty"<<endl;
                l1[newAdd.index][i].dirtyBit = 1;
            }
            else {
                l1[newAdd.index][i].dirtyBit = 0;
            }
            break;
        }
    }
}



//L1 read function
void l1Read(string hexAdd){

    //Get the tag and index
    address newAdd = addMaker(hexAdd, blockSize, l1Sets);
    
    //Attempt L1 Read
    //cout<<"L1 read : "<<newAdd.blockAdd<<" "<<"(tag "<<newAdd.tag<<", "<<"index "<<newAdd.index<<")"<<endl;
    l1Reads += 1;
    for (int i=0; i<l1Assoc; i++){
        if (l1[newAdd.index][i].tag == newAdd.tag && l1[newAdd.index][i].valid==true){
            //cout<<"L1 hit"<<endl;
            //LRU
            if (rPolicy==0){
                //cout<<"L1 update LRU"<<endl;
                l1[newAdd.index][i].serial = l1Serial++;
            }
            if (rPolicy==1){
                //cout<<"L1 update FIFO"<<endl;
            }
            if (rPolicy==2){
                l1[newAdd.index][i].serial = l1Serial++;
                //cout<<"L1 update optimal"<<endl;
            }
            return;
        }
    }
    //Not present in L1
    l1ReadMisses += 1;
    //l2Reads += 1;
    //cout<<"L1 miss"<<endl;

    //Fetch to L1 cache
    //l1Writes += 1;
    l1Write(hexAdd, 0);
}

int main(int argc, char* argv[])
{
    //Take in the initial configuration
    if (argc<8)
    {
        //cout<<"Insufficient arguments"<<"\n";
        //cout<<"Expected arguments 8"<<"\n";
        //cout<<"Supplied arguments "<<argc-1<<"\n"; 
        return(0);
    }

    string blockSizeStr = argv[1];
    string l1SizeStr = argv[2];
    string l1AssocStr = argv[3];
    string l2SizeStr = argv[4];
    string l2AssocStr = argv[5];
    string rPolicyStr = argv[6];
    string iPolicyStr = argv[7];
    traceFile = argv[8];

    blockSize = stol(blockSizeStr, &sz);
    l1Size = stol(l1SizeStr, &sz);
    l1Assoc = stol(l1AssocStr, &sz);
    l2Size = stol(l2SizeStr, &sz);
    l2Assoc = stol(l2AssocStr, &sz);
    rPolicy = stol(rPolicyStr, &sz);
    iPolicy = stol(iPolicyStr, &sz);

    //Print the initial configuration
    cout<<"===== Simulator configuration ====="<<"\n";
    cout<<"BLOCKSIZE:             "<<blockSize<<"\n";
    cout<<"L1_SIZE:               "<<l1Size<<"\n";
    cout<<"L1_ASSOC:              "<<l1Assoc<<"\n";
    cout<<"L2_SIZE:               "<<l2Size<<"\n";
    cout<<"L2_ASSOC:              "<<l2Assoc<<"\n";
    if (rPolicy==0){
        cout<<"REPLACEMENT POLICY:    "<<"LRU"<<"\n";
    }
    else if (rPolicy==1){
        cout<<"REPLACEMENT POLICY:    "<<"FIFO"<<"\n";
    }
    else if (rPolicy==2){
        cout<<"REPLACEMENT POLICY:    "<<"optimal"<<"\n";
    }
    
    if (iPolicy==0){
        cout<<"INCLUSION PROPERTY:    "<<"non-inclusive"<<"\n";
    }
    else if (iPolicy==1){
        cout<<"INCLUSION PROPERTY:    "<<"inclusive"<<"\n";
    }
    
    cout<<"trace_file:            "<<traceFile<<"\n";

    //Create L1 Cache
    l1Sets = l1Size / (blockSize*l1Assoc);
    l1 = new block*[l1Sets];
    for (int i=0; i<l1Sets; i++){
        l1[i] = new block[l1Assoc];
    }
    //Set default values
    for (int i=0; i<l1Sets; i++){
        for (int j=0; j<l1Assoc; j++){
            l1[i][j].blockAdd = "";
            l1[i][j].hexAdd = "";
            l1[i][j].index = i;
            l1[i][j].tag = "";
            l1[i][j].valid = false;
            l1[i][j].serial = 0;
            l1[i][j].dirtyBit = 0;
        }
    }

    //Create L2 Cache
    if (l2Size>0){
        l2Sets = l2Size / (blockSize*l2Assoc);
        l2 = new block*[l2Sets];
        for (int i=0; i<l2Sets; i++){
            l2[i] = new block[l2Assoc];
        }
        //Set default values
        for (int i=0; i<l2Sets; i++){
            for (int j=0; j<l2Assoc; j++){
                l2[i][j].blockAdd = "";
                l2[i][j].hexAdd = "";
                l2[i][j].index = i;
                l2[i][j].tag = "";
                l2[i][j].valid = false;
                l2[i][j].serial = 0;
                l2[i][j].dirtyBit = 0;
            }
        }
    }

    //Open and read trace file
    readTraceFile(traceFile);
    for(filePointer = 0; filePointer<traceFileContent.size(); filePointer++){
        string op = traceFileContent[filePointer]->ops;
        string hexAdd = traceFileContent[filePointer]->hexAdd;
        if (op=="r"){
            //cout<<"----------------------------------------"<<endl;
            //cout<<"#"<<" "<<opCount++<<" "<<":"<<" "<<"read"<<" "<<hexAdd<<endl;
            //l1Reads += 1;
            l1Read(hexAdd);
           
        }
        if (op=="w"){
            //cout<<"----------------------------------------"<<endl;
            //cout<<"#"<<" "<<opCount++<<" "<<":"<<" "<<"write"<<" "<<hexAdd<<endl;
            //l1Writes += 1;
            l1Write(hexAdd, 1);
            
        }
    }

    //Print L1 content
    cout<<"===== L1 contents ====="<<endl;
    for (int i=0; i<l1Sets; i++){
        cout<<"Set"<<"\t"<<i<<":"<<"\t";
        for (int j=0; j<l1Assoc; j++){
            cout<<l1[i][j].tag;
            if (l1[i][j].dirtyBit == 1){
                cout<<" "<<"D";
            }
            cout<<"\t";
        }
        cout<<endl;
    }

    //Print L2 content
    if (l2Size>0){
        cout<<"===== L2 contents ====="<<endl;
        for (int i=0; i<l2Sets; i++){
            cout<<"Set"<<"\t"<<i<<":"<<"\t";
            for (int j=0; j<l2Assoc; j++){
                cout<<l2[i][j].tag;
                if (l2[i][j].dirtyBit == 1){
                    cout<<" "<<"D\t";
                }
                else{
                    cout<<"\t";
                }
            }
            cout<<endl;
        }
    }

    //Calculations
    float l1MissRate = float(l1ReadMisses + l1WriteMisses) / float(l1Reads + l1Writes);
    float l2MissRate = 0;
    if (l2Size>0){
        l2MissRate = float(l2ReadMisses)/float(l2Reads);
    }
    
    if (l2Size==0){
        totalMemoryTraffic = l1ReadMisses + l1WriteMisses + l1Writebacks;
    }

    if (l2Size>0 && iPolicy==0){
        totalMemoryTraffic = l2ReadMisses + l2WriteMisses + l2WriteBacks;
    }

    if (l2Size>0 && iPolicy==1){
        totalMemoryTraffic = l2ReadMisses + l2WriteMisses + l2WriteBacks + l1DirectWriteBacks;
    }


    //Simulation Results
    cout<<"===== Simulation results (raw) ====="<<endl;
    cout<<"a. number of L1 reads:        "<<l1Reads<<endl;
    cout<<"b. number of L1 read misses:  "<<l1ReadMisses<<endl;
    cout<<"c. number of L1 writes:       "<<l1Writes<<endl;
    cout<<"d. number of L1 write misses: "<<l1WriteMisses<<endl;
    cout<<"e. L1 miss rate:              "<<fixed<<setprecision(6)<<l1MissRate<<endl;
    cout<<"f. number of L1 writebacks:   "<<l1Writebacks<<endl;
    cout<<"g. number of L2 reads:        "<<l2Reads<<endl;
    cout<<"h. number of L2 read misses:  "<<l2ReadMisses<<endl;
    cout<<"i. number of L2 writes:       "<<l2Writes<<endl;
    cout<<"j. number of L2 write misses: "<<l2WriteMisses<<endl;
    if (l2Size>0){
        cout<<"k. L2 miss rate:              "<<fixed<<setprecision(6)<<l2MissRate<<endl;
    }
    else{
        cout<<"k. L2 miss rate:              "<<int(l2MissRate)<<endl;
    }
    cout<<"l. number of L2 writebacks:   "<<l2WriteBacks<<endl;
    cout<<"m. total memory traffic:      "<<totalMemoryTraffic<<endl;
}