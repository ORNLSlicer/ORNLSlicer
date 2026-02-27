import json
import sys
import os
import pandas

"""
    Summary: Script converts .ods file to json for the generation of master.conf
    Parameters: [1] file name of .ods file
                [2] file name of output file; must include desired extension

    If new columns are added to .ods file:
        (1) add type of column contents to `types` dictionary
        (2) if column is a string type and needs to be well-formed json,
            add column name to `json_cols` array
"""

# types for all columns
# use type 'object' if column contains multiple types
# other options: 'string', 'bool', 'int', 'float', any other python type
types = {
    'display': 'string',
    'type': 'string',
    'tooltip': 'string',
    'depends': 'string',
    'options': 'string',
    'default': 'object',
    'minor': 'string',
    'major': 'string',
    'namespace': 'string',
    'symbol': 'string',
    'local': 'bool'
}

# list of columns whose strings must be valid json
json_cols = ['depends']


def main():
    n = len(sys.argv)

    # check script parameters
    if n != 3:
        raise(Exception("Incorrect number of arguments. Must be exactly two"))

    ods_file = os.path.normpath(sys.argv[1])
    destination_file = os.path.normpath(sys.argv[2])

    if ods_file[-4:] != '.ods':
        raise(Exception("Unsupported file type. First argument must be '.ods' file."))

    # import the first sheet of the ods file specified by the first command line argument
    data_table = pandas.read_excel(ods_file, engine="odf", na_filter=False)

    data_table.set_index('name', inplace=True)

    # correct all the other types from mis-tytping on import
    data_table = data_table.astype(types)

    # used to make sure the last setting doesn't get a trailing comma
    last_row_index = data_table.iloc[-1].name

    # open the text file
    final_json_file = open(destination_file, "wt", newline="\n")

    # write the opening curly brace
    final_json_file.write("{\n")

    # write out each setting json
    for index, row in data_table.iterrows():

        # if the column is type "object", then it contains multiple types
        # so need to correct each type before converting to json
        for col, value in row.items():
            if types[col] == 'object':
                row[col] = objectToCorrectType(value)

        # convert to json
        current_row = row.to_json(orient="index", indent=4)

        # if the string needs to be json, then replace it with
        # the same string without beginning and trailing quotes and escape characters
        # ex.: "{ \"infill\": \"hexagon\" }" --> { "infill": "hexagon"}
        for col, value in row.items():
            if col in json_cols:
                escaped_str = json.dumps(row[col])
                non_escaped_str = row[col]

                # only replace the 'escaped string' if it actually has info & escape chars
                if escaped_str != '""':
                    current_row = current_row.replace(escaped_str, non_escaped_str)

        # json conversion uses escaped character for '/' for some reason, so undo that
        current_row = current_row.replace("\\/", "/")

        # add level of indentation for easier readability
        current_row = current_row.replace("\n", "\n  ")

        # combine pieces of json
        json_str = '  "' + index + '": ' + current_row

        final_json_file.write(json_str)

        # write a comma if there is more json to write out
        if index != last_row_index:
            final_json_file.write(",\n")

    # write the closing brace
    final_json_file.write("\n}")
    final_json_file.close()


def objectToCorrectType(x):
    if type(x) is str:
        if x == "true" or x == '"true"':   # convert to boolean
            result = True
        elif x == "false" or x == '"false"':
            result = False
        elif len(x) > 0 and x[0] == '[' and x[-1] == ']':  # convert to list
            temp = x.replace('"', '')[1:-1]
            result = temp.split(",")
        else:
            result = x  # happens when x is empty string
    elif type(x) is float:
        if x.is_integer():  # convert to int
            result = int(x)
        else:
            result = x
    else:
        result = x  # shouldn't happen

    return result


if __name__ == '__main__':
    main()
