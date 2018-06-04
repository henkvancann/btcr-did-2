#include <bitcoinapi/types.h>
#include <iostream>
#include "libtxref/txref.h"
#include "txid2txref.h"
#include "bitcoinRPCFacade.h"
#include "t2tSupport.h"

namespace t2t {

    void encodeTxid(const BitcoinRPCFacade &btc, const Config &config, struct Transaction &transaction) {

        blockchaininfo_t blockChainInfo = btc.getblockchaininfo();

        // determine what network we are on
        bool isTestnet = blockChainInfo.chain == "test";

        // use txid to call getrawtransaction to find the blockhash
        getrawtransaction_t rawTransaction = btc.getrawtransaction(config.query, 1);
        std::string blockHash = rawTransaction.blockhash;

        // use blockhash to call getblock to find the block height
        blockinfo_t blockInfo = btc.getblock(blockHash);
        int blockHeight = blockInfo.height;

        // warn if #confirmations are too low
        int numConfirmations = blockInfo.confirmations;
        if (numConfirmations < 6) {
            std::cout << "Warning: 6 confirmations are required for a valid txref: only "
                      << numConfirmations << " found." << std::endl;
        }

        // go through block's transaction array to find transaction position
        std::vector<std::string> blockTransactions = blockInfo.tx;
        std::vector<std::string>::size_type blockIndex;
        for (blockIndex = 0; blockIndex < blockTransactions.size(); ++blockIndex) {
            std::string blockTxid = blockTransactions.at(blockIndex);
            if (blockTxid == config.query)
                break;
        }

        if (blockIndex == blockTransactions.size()) {
            std::cerr << "Error: Could not find transaction " << config.query
                      << " within the block." << std::endl;
            std::exit(-1);
        }

        // TODO need to verify that the uxtoIndex provided on command line is valid

        // call txref code with block height, transaction position, and utxoIndex (if provided) to get txref
        std::string txref;
        if (isTestnet) {
            txref = txref::encodeTestnet(
                    blockHeight, static_cast<int>(blockIndex), config.utxoIndex, config.forceExtended);
        } else {
            txref = txref::encode(
                    blockHeight, static_cast<int>(blockIndex), config.utxoIndex, config.forceExtended);
        }

        // output
        transaction.query = config.query;
        transaction.txid = config.query;
        transaction.txref = txref;
        transaction.blockHeight = blockHeight;
        transaction.position = static_cast<int>(blockIndex);
        transaction.network = blockChainInfo.chain;
        transaction.utxoIndex = config.utxoIndex;
    }

    void decodeTxref(const BitcoinRPCFacade &btc, const Config &config, struct Transaction &transaction) {

        txref::LocationData locationData = txref::decode(config.query);

        blockchaininfo_t blockChainInfo = btc.getblockchaininfo();

        // determine what network we are on
        bool isTestnet = blockChainInfo.chain == "test";

        // get block hash for block at location "height"
        std::string blockHash = btc.getblockhash(locationData.blockHeight);

        // use block hash to get the block
        blockinfo_t blockInfo = btc.getblock(blockHash);

        // get the txid from the transaction at "position"
        std::string txid;
        try {
            txid = blockInfo.tx.at(locationData.transactionPosition);
        }
        catch (std::out_of_range &e) {
            std::cerr << "Error: Could not find transaction " << config.query
                      << " within the block." << std::endl;
            std::exit(-1);
        }

        // output
        transaction.query = config.query;
        transaction.txid = txid;
        transaction.txref = locationData.txref;
        transaction.blockHeight = locationData.blockHeight;
        transaction.position = locationData.transactionPosition;
        transaction.utxoIndex = locationData.uxtoIndex;
        transaction.network = blockChainInfo.chain;
    }

}