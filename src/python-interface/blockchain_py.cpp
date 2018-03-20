//
//  blockchain_py.cpp
//  blocksci
//
//  Created by Harry Kalodner on 7/4/17.
//
//

#include "variant_py.hpp"
#include "optional_py.hpp"

#include <blocksci/chain/algorithms.hpp>
#include <blocksci/chain/blockchain.hpp>
#include <blocksci/chain/transaction.hpp>
#include <blocksci/index/address_index.hpp>
#include <blocksci/index/hash_index.hpp>
#include <blocksci/scripts/script_variant.hpp>
#include <blocksci/heuristics/blockchain_heuristics.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <range/v3/view/any_view.hpp>
#include <range/v3/view/slice.hpp>
#include <range/v3/view/stride.hpp>

namespace py = pybind11;

using namespace blocksci;

void init_blockchain(py::module &m) {
    
    py::class_<DataConfiguration> (m, "DataConfiguration", "This class holds the configuration data about a blockchain instance")
    .def(py::pickle(
        [](const DataConfiguration &config) {
            return py::make_tuple(config.dataDirectory.native(), config.errorOnReorg, config.blocksIgnored);
        },
        [](py::tuple t) {
            if (t.size() != 3) {
                throw std::runtime_error("Invalid state!");
            }
            return DataConfiguration(t[0].cast<std::string>(), t[1].cast<bool>(), t[2].cast<BlockHeight>());
        }
    ))
    ;
    
    py::class_<Blockchain> cl(m, "Blockchain", "Class representing the blockchain. This class is contructed by passing it a string representing a file path to your BlockSci data files generated by blocksci_parser", py::dynamic_attr());
    cl
    .def(py::init<std::string>())
    .def(py::init<DataConfiguration>())
    .def("__len__", [](const Blockchain &chain) {
        return chain.size();
    }, "Returns the total number of blocks in the blockchain")
    /// Optional sequence protocol operations
    .def("__iter__", [](const Blockchain &chain) { return py::make_iterator(chain.begin(), chain.end()); },
         py::keep_alive<0, 1>(), "Allows direct iteration over the blocks in the blockchain")
    .def("__getitem__", [](const Blockchain &chain, blocksci::BlockHeight i) {
        if (i < 0) {
            i += chain.size();
        }
        if (i >= chain.size()) {
            throw py::index_error();
        }
        return chain[i];
    }, "Return the block of the given height")
    .def("__getitem__", [](const Blockchain &chain, py::slice slice) -> std::vector<Block> {
        size_t start, stop, step, slicelength;
        if (!slice.compute(static_cast<size_t>(static_cast<int>(chain.size())), &start, &stop, &step, &slicelength))
            throw py::error_already_set();
        return chain | ranges::view::slice(start, stop) | ranges::view::stride(step) | ranges::to_vector;
    }, "Return a list of blocks with their heights in the given range")
    .def_property_readonly("config", [](const Blockchain &chain) -> DataConfiguration { return chain.getAccess().config; }, "Returns the configuration settings for this blockchain")
    .def("segment", segmentChain, "Divide the blockchain into the given number of chunks with roughly the same number of transactions in each")
    .def("segment_indexes", segmentChainIndexes, "Return a list of [start, end] block height pairs representing chunks with roughly the same number of transactions in each")
    .def("address_count", &Blockchain::addressCount, "Get an upper bound of the number of address of a given type (This reflects the number of type equivlant addresses of that type).")
    .def("address_type_txes", getTransactionIncludingOutput, "Returns a list of all transactions that include outputs of the given address type")
    .def("addresses", [](const Blockchain &chain, AddressType::Enum type) {
        return chain.scripts(type);
    })
    .def_property_readonly("outputs_unspent", [](const Blockchain &chain) -> ranges::any_view<Output> { return outputsUnspent(chain); }, "Returns a list of all of the outputs that are unspent")
    .def("tx_with_index", [](const Blockchain &chain, uint32_t index) {
        return Transaction{index, chain.getAccess()};
    }, R"docstring(
         This functions gets the transaction with given index.
         
         :param int index: The index of the transation.
         :returns: Tx
         )docstring")
    .def("tx_with_hash", [](const Blockchain &chain, const std::string &hash) {
        return Transaction{hash, chain.getAccess()};
    }, R"docstring(
         This functions gets the transaction with given hash.
         
         :param string index: The hash of the transation.
         :returns: Tx
         )docstring")
    .def("address_from_index", [](const Blockchain &chain, uint32_t index, AddressType::Enum type) -> AnyScript::ScriptVariant {
        return Address{index, type, chain.getAccess()}.getScript().wrapped;
    }, "Construct an address object from an address num and type")
    .def("address_from_string", [](const Blockchain &chain, const std::string &addressString) -> ranges::optional<AnyScript::ScriptVariant> {
        auto address = getAddressFromString(addressString, chain.getAccess());
        if (address) {
            return address->getScript().wrapped;
        } else {
            return ranges::nullopt;
        }
    }, "Construct an address object from an address string")
    .def("addresses_with_prefix", [](const Blockchain &chain, const std::string &addressPrefix) {
        py::list pyAddresses;
        auto addresses = getAddressesWithPrefix(addressPrefix, chain.getAccess());
        for (auto &address : addresses) {
            pyAddresses.append(address.getScript().wrapped);
        }
        return pyAddresses;
    }, "Find all addresses beginning with the given prefix")
    ;
}