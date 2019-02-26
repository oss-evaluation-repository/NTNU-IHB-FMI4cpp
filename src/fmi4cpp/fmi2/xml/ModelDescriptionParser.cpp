/*
 * The MIT License
 *
 * Copyright 2017-2018 Norwegian University of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING  FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <fmi4cpp/fmi2/xml/ModelDescriptionParser.hpp>
#include <fmi4cpp/fmi2/xml/ScalarVariableAttribute.hpp>

#include "../../common/tools/optional_converter.hpp"

using boost::property_tree::ptree;
using namespace fmi4cpp::fmi2;

namespace {

    const std::string DEFAULT_VARIABLE_NAMING_CONVENTION = "flat";

    const DefaultExperiment parseDefaultExperiment(const ptree &node) {
        DefaultExperiment ex;
        ex.startTime = convert(node.get_optional<double>("<xmlattr>.startTime"));
        ex.stopTime = convert(node.get_optional<double>("<xmlattr>.stopTime"));
        ex.stepSize = convert(node.get_optional<double>("<xmlattr>.stepSize"));
        ex.tolerance = convert(node.get_optional<double>("<xmlattr>.tolerance"));
        return ex;
    }

    const SourceFile parseFile(const ptree &node) {
        SourceFile file;
        file.name = node.get<std::string>("<xmlattr>.name");
        return file;
    }

    void parseSourceFiles(const ptree &node, SourceFiles &files) {
        for (const ptree::value_type &v : node) {
            if (v.first == "File") {
                auto file = parseFile(v.second);
                files.push_back(file);
            }
        }
    }

    void parseUnknownDependencies(const std::string &str, std::vector<unsigned int> &store) {
        unsigned int i;
        std::stringstream ss(str);
        while (ss >> i) {
            store.push_back(i);
            if (ss.peek() == ',' || ss.peek() == ' ') {
                ss.ignore();
            }
        }
    }

    void parseUnknownDependenciesKind(const std::string &str, std::vector<std::string> &store) {
        boost::split(store, str, [](char c) { return c == ' '; });
    }

    const Unknown parseUnknown(const ptree &node) {

        Unknown unknown;
        unknown.index = node.get<unsigned int>("<xmlattr>.index");

        auto opt_dependencies = node.get_optional<std::string>("<xmlattr>.dependencies");
        if (opt_dependencies) {
            std::vector<unsigned int> dependencies;
            parseUnknownDependencies(*opt_dependencies, dependencies);
            unknown.dependencies = dependencies;
        }

        auto opt_dependenciesKind = node.get_optional<std::string>("<xmlattr>.dependenciesKind");
        if (opt_dependenciesKind) {
            std::vector<std::string> dependenciesKind;
            parseUnknownDependenciesKind(*opt_dependenciesKind, dependenciesKind);
            unknown.dependenciesKind = dependenciesKind;
        }

        return unknown;
    }

    void loadUnknowns(const ptree &node, std::vector<Unknown> &vector) {
        for (const ptree::value_type &v : node) {
            if (v.first == "Unknown") {
                auto unknown = parseUnknown(v.second);
                vector.push_back(unknown);
            }
        }
    }

    std::unique_ptr<const ModelStructure> parseModelStructure(const ptree &node) {

        std::vector<Unknown> outputs;
        std::vector<Unknown> derivatives;
        std::vector<Unknown> initialUnknowns;

        for (const ptree::value_type &v : node) {
            if (v.first == "Outputs") {
                loadUnknowns(v.second, outputs);
            } else if (v.first == "Derivatives") {
                loadUnknowns(v.second, derivatives);
            } else if (v.first == "InitialUnknowns") {
                loadUnknowns(v.second, initialUnknowns);
            }
        }

        return std::make_unique<const ModelStructure>(outputs, derivatives, initialUnknowns);

    }

    FmuAttributes parseFmuAttributes(const ptree &node) {

        FmuAttributes attributes;

        attributes.modelIdentifier = node.get<std::string>("<xmlattr>.modelIdentifier");
        attributes.needsExecutionTool = node.get<bool>("xmlattr>.needsExecutionTool", false);
        attributes.canGetAndSetFMUstate = node.get<bool>("xmlattr>.canGetAndSetFMUstate", false);
        attributes.canSerializeFMUstate = node.get<bool>("xmlattr>.canSerializeFMUstate", false);
        attributes.providesDirectionalDerivative = node.get<bool>("xmlattr>.providesDirectionalDerivative", false);
        attributes.canNotUseMemoryManagementFunctions = node.get<bool>("xmlattr>.canNotUseMemoryManagementFunctions",
                                                                       false);
        attributes.canBeInstantiatedOnlyOncePerProcess = node.get<bool>("xmlattr>.canBeInstantiatedOnlyOncePerProcess",
                                                                        false);

        for (const ptree::value_type &v : node) {
            if (v.first == "SourceFiles") {
                parseSourceFiles(v.second, attributes.sourceFiles);
            }
        }

        return attributes;

    }

    const CoSimulationAttributes parseCoSimulationAttributes(const ptree &node) {

        CoSimulationAttributes attributes(parseFmuAttributes(node));
        attributes.maxOutputDerivativeOrder = node.get<unsigned int>("<xmlattr>.maxOutputDerivativeOrder", 0);
        attributes.canInterpolateInputs = node.get<bool>("<xmlattr>.canInterpolateInputs", false);
        attributes.canRunAsynchronuously = node.get<bool>("<xmlattr>.canRunAsynchronuously", false);
        attributes.canHandleVariableCommunicationStepSize = node.get<bool>(
                "<xmlattr>.canHandleVariableCommunicationStepSize",
                false);
        return attributes;

    }

    const ModelExchangeAttributes parseModelExchangeAttributes(const ptree &node) {
        ModelExchangeAttributes attributes(parseFmuAttributes(node));
        attributes.completedIntegratorStepNotNeeded = node.get<bool>("<xmlattr>.completedIntegratorStepNotNeeded",
                                                                     false);
        return attributes;
    }

    template<typename T>
    ScalarVariableAttribute<T> parseScalarVariableAttributes(const ptree &node) {
        ScalarVariableAttribute<T> attributes;
        attributes.start = convert(node.get_optional<T>("<xmlattr>.start"));
        attributes.declaredType = convert(node.get_optional<std::string>("<xmlattr>.declaredType"));
        return attributes;
    }

    template<typename T>
    BoundedScalarVariableAttribute<T> parseBoundedScalarVariableAttributes(const ptree &node) {
        BoundedScalarVariableAttribute<T> attributes(parseScalarVariableAttributes<T>(node));
        attributes.min = convert(node.get_optional<T>("<xmlattr>.min"));
        attributes.max = convert(node.get_optional<T>("<xmlattr>.max"));
        attributes.quantity = convert(node.get_optional<std::string>("<xmlattr>.quantity"));
        return attributes;
    }

    IntegerAttribute parseIntegerAttribute(const ptree &node) {
        return IntegerAttribute(parseBoundedScalarVariableAttributes<int>(node));
    }

    RealAttribute parseRealAttribute(const ptree &node) {
        RealAttribute attributes(parseBoundedScalarVariableAttributes<double>(node));
        attributes.nominal = convert(node.get_optional<double>("<xmlattr>.nominal"));
        attributes.unit = convert(node.get_optional<std::string>("<xmlattr>.unit"));
        attributes.derivative = convert(node.get_optional<unsigned int>("<xmlattr>.derivative"));
        attributes.reinit = node.get<bool>("<xmlattr>.reinit", false);
        attributes.unbounded = node.get<bool>("<xmlattr>.unbounded", false);
        attributes.relativeQuantity = node.get<bool>("<xmlattr>.relativeQuantity", false);
        return attributes;
    }

    StringAttribute parseStringAttribute(const ptree &node) {
        return StringAttribute(parseScalarVariableAttributes<std::string>(node));
    }

    BooleanAttribute parseBooleanAttribute(const ptree &node) {
        return BooleanAttribute(parseScalarVariableAttributes<bool>(node));
    }

    EnumerationAttribute parseEnumerationAttribute(const ptree &node) {
        return EnumerationAttribute(parseBoundedScalarVariableAttributes<int>(node));
    }


    const ScalarVariable parseScalarVariable(const ptree &node) {

        ScalarVariableBase base;

        base.name = node.get<std::string>("<xmlattr>.name");
        base.description = node.get<std::string>("<xmlattr>.description", "");
        base.valueReference = node.get<fmi2ValueReference>("<xmlattr>.valueReference");
        base.canHandleMultipleSetPerTimelnstant = node.get<bool>("<xmlattr>.canHandleMultipleSetPerTimelnstant", false);

        base.causality = parseCausality(node.get<std::string>("<xmlattr>.causality", ""));
        base.variability = parseVariability(node.get<std::string>("<xmlattr>.variability", ""));
        base.initial = parseInitial(node.get<std::string>("<xmlattr>.initial", ""));

        for (const ptree::value_type &v : node) {
            if (v.first == INTEGER_TYPE) {
                return ScalarVariable(base, parseIntegerAttribute(v.second));
            } else if (v.first == REAL_TYPE) {
                return ScalarVariable(base, parseRealAttribute(v.second));
            } else if (v.first == STRING_TYPE) {
                return ScalarVariable(base, parseStringAttribute(v.second));
            } else if (v.first == BOOLEAN_TYPE) {
                return ScalarVariable(base, parseBooleanAttribute(v.second));
            } else if (v.first == ENUMERATION_TYPE) {
                return ScalarVariable(base, parseEnumerationAttribute(v.second));
            }
        }

        throw std::runtime_error("FATAL: Failed to parse ScalarVariable!");

    }

    std::unique_ptr<const ModelVariables> parseModelVariables(const ptree &node) {
        std::vector<ScalarVariable> variables;
        for (const ptree::value_type &v : node) {
            if (v.first == "ScalarVariable") {
                auto var = parseScalarVariable(v.second);
                variables.push_back(var);
            }
        }
        return std::make_unique<const ModelVariables>(variables);
    }

}

std::unique_ptr<const ModelDescription> fmi4cpp::fmi2::parseModelDescription(const std::string &fileName) {

    ptree tree;
    read_xml(fileName, tree);
    ptree root = tree.get_child("fmiModelDescription");

    ModelDescriptionBase base;

    base.guid = root.get<std::string>("<xmlattr>.guid");
    base.fmiVersion = root.get<std::string>("<xmlattr>.fmiVersion");
    base.modelName = root.get<std::string>("<xmlattr>.modelName");
    base.description = root.get<std::string>("<xmlattr>.description", "");
    base.author = root.get<std::string>("<xmlattr>.author", "");
    base.version = root.get<std::string>("<xmlattr>.version", "");
    base.license = root.get<std::string>("<xmlattr>.license", "");
    base.copyright = root.get<std::string>("<xmlattr>.copyright", "");
    base.generationTool = root.get<std::string>("<xmlattr>.generationTool", "");
    base.generationDateAndTime = root.get<std::string>("<xmlattr>.generationDateAndTime", "");
    base.numberOfEventIndicators = root.get<size_t>("<xmlattr>.numberOfEventIndicators", 0);
    base.variableNamingConvention = root.get<std::string>("<xmlattr>.variableNamingConvention",
                                                          DEFAULT_VARIABLE_NAMING_CONVENTION);

    std::optional<CoSimulationAttributes> coSimulation;
    std::optional<ModelExchangeAttributes> modelExchange;

    for (const ptree::value_type &v : root) {

        if (v.first == "CoSimulation") {
            coSimulation = parseCoSimulationAttributes(v.second);
        } else if (v.first == "ModelExchange") {
            modelExchange = parseModelExchangeAttributes(v.second);
        } else if (v.first == "DefaultExperiment") {
            base.defaultExperiment = parseDefaultExperiment(v.second);
        } else if (v.first == "ModelVariables") {
            base.modelVariables = std::move(parseModelVariables(v.second));
        } else if (v.first == "ModelStructure") {
            base.modelStructure = std::move(parseModelStructure(v.second));
        }

    }

    return std::make_unique<const ModelDescription>(base, coSimulation, modelExchange);

}

