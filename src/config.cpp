/*
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.hpp"

#include <iostream>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/bitcoin.hpp>
#include "settings.hpp"

namespace libbitcoin {
namespace server {

using boost::format;
using boost::filesystem::path;
using namespace boost::program_options;

static path get_config_option(variables_map& variables)
{
    // read config from the map so we don't require an early notify
    const auto& config = variables[BS_CONFIGURATION_VARIABLE];

    // prevent exception in the case where the config variable is not set
    if (config.empty())
        return path();

    return config.as<path>();
}

static void load_command_variables(variables_map& variables,
    config_type& metadata, int argc, const char* argv[]) throw()
{
    const auto options = metadata.load_options();
    const auto arguments = metadata.load_arguments();
    auto command_parser = command_line_parser(argc, argv).options(options)
        .positional(arguments);
    store(command_parser.run(), variables);
}

// Not unit testable (without creating actual config files).
static bool load_configuration_variables(variables_map& variables,
    config_type& metadata) throw(reading_file)
{
    const auto config_path = get_config_option(variables);
    if (!config_path.empty())
    {
        const auto& path = config_path.generic_string();
        std::ifstream file(path);
        if (file.good())
        {
            const auto config_settings = metadata.load_settings();
            const auto config = parse_config_file(file, config_settings);
            store(config, variables);
            return true;
        }
    }

    // loading from an empty stream causes the defaults to populate
    std::stringstream stream;
    const auto config_settings = metadata.load_settings();
    const auto config = parse_config_file(stream, config_settings);
    store(config, variables);
    return false;
}

static void load_environment_variables(variables_map& variables, 
    config_type& metadata) throw()
{
    const auto& environment_variables = metadata.load_environment();
    const auto environment = parse_environment(environment_variables,
        BS_ENVIRONMENT_VARIABLE_PREFIX);
    store(environment, variables);
}

bool load_config(config_type& metadata, std::string& message, int argc, 
    const char* argv[])
{
    try
    {
        variables_map variables;
        load_command_variables(variables, metadata, argc, argv);

        // Must store before configuration in order to use config path.
        load_environment_variables(variables, metadata);

        // Returns true if the settings were loaded from a file.
        bool loaded_file = load_configuration_variables(variables, metadata);

        // Update bound variables in metadata.settings.
        notify(variables);

        // Clear the config file path if it wasn't used.
        if (!loaded_file)
            metadata.settings.config.clear();
    }
    catch (const boost::program_options::error& e)
    {
        // This assumes boost exceptions are not disabled. Doing so doesn't
        // actually prevent program_options from throwing, so we avoid the 
        // complexity of handling that configuration here.

        // This is obtained from boost, which circumvents our localization.
        message = e.what();
        return false;
    }
    catch (const boost::exception&)
    {
        message = "boost::exception";
        return false;
    }
    catch (const std::exception& e)
    {
        message = e.what();
        return false;
    }

    return true;
}

} // namespace server
} // namespace libbitcoin
