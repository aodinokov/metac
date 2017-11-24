#!/usr/bin/gawk -f

function mark_obj(id) {
    if ("marked" in data[id])
        return;
    data[id]["marked"] = 1;
    if ("child" in data[id])
        for (c in data[id]["child"])
            mark_obj(data[id]["child"][c]);
    if ("DW_AT_type" in data[id]) {
        match(data[id]["DW_AT_type"], /<([^>]+)>/, ref_type_data);
        mark_obj(ref_type_data[1]);
    }
}

function dump_dwarf_at_data(type, i) {
    res = "";
    at_rows = 0;
    at_num = 0;
    at_res = "\t.at = (metac_type_at_t[]) {\n";
    for (j in data[i]) {
        if (match(j, "DW_AT_(.*)", arr0)) {
            #intentionally don't do check of attributes and type - compiler will do it and compilation will fail if we have a problem
            switch(j){
                case "DW_AT_name":
                    at_res = at_res "\t\t{.id = " j ", ." arr0[1] " = \"" data[i][j] "\"},\n";
                    ++at_num;
                    break;
                case "DW_AT_data_member_location":
                    if (match(data[i][j], "([0-9]+).*", arr)) {
                        at_res = at_res "\t\t{.id = " j ", ." arr0[1] " = " arr[1] "/*" data[i][j] "*/},\n";
                        ++at_num;
                    }
                    break;
                case "DW_AT_type":
                    if (match(data[i][j], "<([^>]+)>", arr)) {
                        at_res = at_res "\t\t{.id = " j ", ." arr0[1] " = &" type_variable_name(arr[1]) "},\n";
                        ++at_num;
                    }
                    break;
                case "DW_AT_byte_size":
                case "DW_AT_bit_offset":
                case "DW_AT_bit_size":
                case "DW_AT_encoding":
                case "DW_AT_lower_bound":
                case "DW_AT_upper_bound":
                case "DW_AT_count":
                case "DW_AT_const_value":
                    at_res = at_res "\t\t{.id = " j ", ." arr0[1] " = " data[i][j] "},\n"
                    ++at_num;
                    break;
                default:
                    at_res = at_res "\t\t/* Skip {.id = " j ", ." arr0[1] " = " data[i][j] "}, */\n"
            }
            ++at_rows;
        }
    }
    at_res = at_res "\t},"
    if (at_num > 0)
        res = res "\t.at_num = " at_num ",\n";
    if (at_rows > 0)
        res = res at_res;
    return res;
}

function dump_dwarf_child_data(type, i) {
    res = "";
    if ("child" in data[i]) {
        child_num = 0;
        child_res = "\t.child = (struct metac_type*[]) {\n"
        for (k in data[i]["child"]) {
            child_res = child_res "\t\t&" type_variable_name(data[i]["child"][k]) ",\n";
            ++child_num;
        }
        child_res = child_res "\t},"
        res = res "\t.child_num = " child_num ",\n" child_res;
    }
    return res;
}

function dump_main_types(type, i) {
    res = "";
    for (j in data[i]) {
        if (match(j, "DW_AT_(.*)", arr0)) {
            #intentionally don't do check of attributes and type - compiler will do it and compilation will fail if we have a problem
            switch(j){
            case "DW_AT_name":
                res = res "\t\t." arr0[1] " = \"" data[i][j] "\",\n";
                break;
            case "DW_AT_data_member_location":
                if (match(data[i][j], "([0-9]+).*", arr)) {
                    res = res "\t\t." arr0[1] " = " arr[1] "/*" data[i][j] "*/,\n";
                }
                break;
            case "DW_AT_type":
                if (match(data[i][j], "<([^>]+)>", arr)) {
                    res = res "\t\t." arr0[1] " = &" type_variable_name(arr[1]) ",\n";
                }
                break;
            case "DW_AT_byte_size":
            case "DW_AT_bit_offset":
            case "DW_AT_bit_size":
            case "DW_AT_encoding":
            case "DW_AT_lower_bound":
            case "DW_AT_upper_bound":
            case "DW_AT_count":
            case "DW_AT_const_value":
                res = res "\t\t." arr0[1] " = " data[i][j] ",\n"
                break;
            default:
                #res = res "\t\t/* Skip ." arr0[1] " = " data[i][j] ", */\n"
            }
        }
    }
    # processing children for some types
    switch (type) {
        case "enumeration_type":
            if ("child" in data[i]) {
                count = 0;
                res1 = "\t\t.enumerators = (struct enumerator_info[]) {\n"
                for (k in data[i]["child"]) {
                    child_i = data[i]["child"][k];
                    if (data[child_i]["type"] == "DW_TAG_enumerator") {
                        res1 = res1 "\t\t\t{.name = \"" data[child_i]["DW_AT_name"] "\", .const_value = " data[child_i]["DW_AT_const_value"] "},\n";
                        ++count;
                    }
                }
                res1 = res1 "\t\t},\n"
                res = res "\t\t.enumerators_count = "count ",\n" res1;
            }
        break;
        case "subprogram":
            if ("child" in data[i]) {
                count = 0;
                res1 = "\t\t.parameters = (struct subprogram_parameter[]) {\n"
                for (k in data[i]["child"]) {
                    child_i = data[i]["child"][k];
                    switch (data[child_i]["type"]) {
                    case "DW_TAG_formal_parameter":
                        if (match(data[child_i]["DW_AT_type"], "<([^>]+)>", arr)) {
                            res1 = res1 "\t\t\t{.name = \"" data[child_i]["DW_AT_name"] "\", .type = &" type_variable_name(arr[1]) "},\n";
                            ++count;
                        }
                        break;
                    case "DW_TAG_unspecified_parameters":
                        res1 = res1 "\t\t\t{.unspecified_parameters = 1},\n";
                        ++count;
                        break;
                    }
                }
                res1 = res1 "\t\t},\n"
                res = res "\t\t.parameters_count = "count ",\n" res1;
            }
        break;
        case "structure_type":
        case "union_type":
            if ("child" in data[i]) {
                count = 0;
                res1 = "\t\t.members = (struct member_info[]) {\n"
                for (k in data[i]["child"]) {
                    child_i = data[i]["child"][k];
                    switch (data[child_i]["type"]) {
                    case "DW_TAG_member":
                        res1 = res1 "\t\t\t{\n";
                        res1 = res1 "\t\t\t\t.name = \"" data[child_i]["DW_AT_name"] "\",\n";
                        if (match(data[child_i]["DW_AT_type"], "<([^>]+)>", arr)) {
                            res1 = res1 "\t\t\t\t.type = &" type_variable_name(arr[1]) ",\n";
                        }
                        #optional attributes:
                        if ("DW_AT_data_member_location" in data[child_i] && 
                             match(data[child_i]["DW_AT_data_member_location"], "([0-9]+).*", arr)) {
                            res1 = res1 "\t\t\t\t.p_data_member_location = (metac_data_member_location_t[]){" arr[1] "/*"data[child_i]["DW_AT_data_member_location"] "*/},\n";
                        }
                        if ("DW_AT_bit_offset" in data[child_i]) {
                            res1 = res1 "\t\t\t\t.p_bit_offset = (metac_bit_offset_t[]){" data[child_i]["DW_AT_bit_offset"] "},\n";
                        }
                        if ("DW_AT_bit_size" in data[child_i]) {
                            res1 = res1 "\t\t\t\t.p_bit_size = (metac_bit_size_t[]){" data[child_i]["DW_AT_bit_size"] "},\n";
                        }
                        res1 = res1 "\t\t\t},\n";
                        ++count;
                        break;
                    }
                }
                res1 = res1 "\t\t},\n"
                res = res "\t\t.members_count = "count ",\n" res1;
            }
        break;        
        case "array_type":
            # analogy of enumberation_type + some precalculated params (number of elements and etc)
            if ("child" in data[i]) {
                count = 0;
                elements_count = "1";
                is_flexible = 0;
                res1 = "\t\t.subranges = (struct subrange_info []) {\n"
                for (k in data[i]["child"]) {
                    child_i = data[i]["child"][k];
                    switch (data[child_i]["type"]) {
                    case "DW_TAG_subrange_type":
                        res1 = res1 "\t\t\t{\n";
                        if (match(data[child_i]["DW_AT_type"], "<([^>]+)>", arr)) {
                            res1 = res1 "\t\t\t\t.type = &" type_variable_name(arr[1]) ",\n";
                        }
                        #optional attributes:
                        if ("DW_AT_count" in data[child_i]) {
                            res1 = res1 "\t\t\t\t.p_count = (metac_count_t[]){" data[child_i]["DW_AT_count"] "},\n";
                        }
                        if ("DW_AT_lower_bound" in data[child_i]) {
                            res1 = res1 "\t\t\t\t.p_lower_bound = (metac_bound_t[]){" data[child_i]["DW_AT_lower_bound"] "},\n";
                        }
                        if ("DW_AT_upper_bound" in data[child_i]) {
                            res1 = res1 "\t\t\t\t.p_upper_bound = (metac_bound_t[]){" data[child_i]["DW_AT_upper_bound"] "},\n";
                        }
                        res1 = res1 "\t\t\t},\n";
                        # calc length
                        if ("DW_AT_count" in data[child_i])
                            elements_count = elements_count " * " data[child_i]["DW_AT_count"]
                        else if ("DW_AT_upper_bound" in data[child_i]) {
                            elements_count = elements_count " * (" data[child_i]["DW_AT_upper_bound"] " + 1";
                            if ("DW_AT_lower_bound" in data[child_i])
                                elements_count = elements_count " - " data[child_i]["DW_AT_lower_bound"];
                            elements_count = elements_count ")";
                        } else {
                            is_flexible = 1;
                        }
                        ++count;
                        break;
                    }
                }
                res1 = res1 "\t\t},\n"
                if (is_flexible == 1)
                    res = res "\t\t.is_flexible = 1,\n"
                else
                    res = res "\t\t.elements_count = " elements_count ",\n"
                res = res "\t\t.subranges_count = "count ",\n" res1;
            }
        break;
    }
    return res;
}

function type_name(name) {
    if (name == "long int")
        return "long";
    if (name == "short int")
        return "short";
    return name;
}

function _type_(addr) {
    return data[addr]["type"];
}

function type_variable_name(addr) {
    if (!("DW_AT_name" in data[addr]) ||
        type == "DW_TAG_member" ||
        type == "DW_TAG_enumerator" ||
        type == "DW_TAG_formal_parameter")
        return "data_" addr;
    
    name = type_name(data[addr]["DW_AT_name"]);
    
    if (!(name in task4types) &&
        !(name in types_array))
        return "data_" addr;
    
    return "metac__type_" gensub(/ /, "_", "g", name);
}

function static_if_needed(addr) {
    if ("DW_AT_name" in data[addr] && type_name(data[addr]["DW_AT_name"]) in task4types)
            return "";
    return "static ";
}

# workaround for old clang: https://travis-ci.org/aodinokov/metac/jobs/194107462
function type_variable_name_for_initializer(addr) {
    # second definition will not pass this if construction
    if ("DW_AT_name" in data[addr] && type_name(data[addr]["DW_AT_name"]) in task4types)
            return type_variable_name(addr);
    return "data_" addr;
}

BEGIN {
    # init empty arrays
    task4types[0] = "";
    delete task4types[0];
    task4specs[0] = "";
    delete task4specs[0];
    task4objects[0] = "";
    delete task4objects[0];
    types_array[0] = "";
    delete types_array[0];

    obj_stack[0] = ""
    if (file) {
        file;
        while(( getline line < file ) > 0 ) {
            split(line, x, " ");
            switch (x[1]) {
            case "type":
                task4types[x[2]] = x[1];
                break;
            case "typespec":
                task4specs[x[2]] = x[1];
                break;
            case "object":
                task4objects[x[2]] = x[1];
                break;
            case "types":
                # we ignore them
                break;
            case "objects":
                # we ignore them
                break;
            default:
                print "WARNING: Unknown task type for line: " line > "/dev/stderr";
                break;
            }
        }
    }
}

END {
    main_type_ids["base_type"] = "base_type";
    main_type_ids["pointer_type"] = "pointer_type";
    main_type_ids["typedef"] = "typedef";
    main_type_ids["enumeration_type"] = "enumeration_type";
    main_type_ids["subprogram"] = "subprogram";
    main_type_ids["structure_type"] = "structure_type";
    main_type_ids["union_type"] = "union_type";
    main_type_ids["array_type"] = "array_type";
    
    for (i in data) {
        if ((length(task4types) == 0) ||
            ("DW_AT_name" in data[i] && type_name(data[i]["DW_AT_name"]) in task4types))
            mark_obj(i);
    }
    
    asort(obj);
    for (i in data) {
        if (data[i]["marked"] == 1) {
            obj[length(obj)] = i;
        }
    }
    asort(obj);
    
    print "/* This file is auto-generated. Don't modify it if you aren't really sure about the reason. */";
    print "#include \"metac_type.h\""
    print "#include <stdio.h>  /* NULL */"
    print "\n/* explicit type parameters declarations */"
    
    for (i in task4specs) {
        print "METAC_TYPE_SPECIFICATION_DECLARE(" i ");";
    }
    
    print "\n\n/* early declaration */"
    
    for (i in obj) {
        i = obj[i];
        print static_if_needed(i) "struct metac_type " type_variable_name(i) "; /* " i " */";
    }
    
    print "\n/* real data */"
    
    for (i in obj) {
        i = obj[i];
        at = "";
        p_at = "";
        
        print "/* --" i "--*/"
        
        print static_if_needed(i) "struct metac_type " type_variable_name_for_initializer(i) " = {";
        if ("type" in data[i]) {
            print "\t.id = " data[i]["type"] ","
        }
        #dwarf data 
        text = dump_dwarf_at_data(arr0[1], i); if (length(text) > 0)print text;
        text = dump_dwarf_child_data(arr0[1], i); if (length(text) > 0)print text;
        # special logic to print everything about the type and its parts
        if ("type" in data[i] && match(data[i]["type"], "DW_TAG_(.*)", arr0) && arr0[1] in main_type_ids) {
            print "\t."arr0[1]"_info = {\n" dump_main_types(arr0[1], i) "\t},";
        }
        #specifications
        if ( type_name(data[i]["DW_AT_name"]) in task4specs) {
            print "\t.specifications = METAC(typespec, " type_name(data[i]["DW_AT_name"]) "),"
        }
        print "};"
        
        # move type from task4types to types_array to print the whole list of types
        if ("DW_AT_name" in data[i] && type_name(data[i]["DW_AT_name"]) in task4types) {
            types_array[type_name(data[i]["DW_AT_name"])] = type_variable_name(i);
            delete task4types[type_name(data[i]["DW_AT_name"])];
        }
        print;
    }
    
    #print whole list of types
    print "struct metac_type_sorted_array METAC_TYPES_ARRAY = {"
    asorti(types_array, types_array_sorted);
    print "\t.number = " length(types_array_sorted) ",";
    print "\t.item = {";
    
    for (i in types_array_sorted) {
        print "\t\t{.name = \"" types_array_sorted[i] "\", .ptr = &" types_array[types_array_sorted[i]] "},"
    }
    
    print "\t},\n};\n";
    
    #print whole list of static objects
    for (i in task4objects) {
        print "extern struct metac_object METAC_OBJECT_NAME(" i ");"
    }
    print
    print "struct metac_object_sorted_array METAC_OBJECTS_ARRAY = {"
    asorti(task4objects, task4objects_sorted);
    
    print "\t.number = " length(task4objects_sorted) ",";
    print "\t.item = {";
    for (i in task4objects_sorted) {
        print "\t\t{.name = \"" task4objects_sorted[i] "\", .ptr = &METAC_OBJECT_NAME(" task4objects_sorted[i] ")},"
    }
    print "\t},\n};\n";
}
/^<[^>]+><[^>]+>/{
    while (match($0, /<([^>]+)><([^>]+)>[ \t]+([^ \t]*)/, arr)) {
        inx = arr[2];
        val = arr[3];
        data[inx]["type"] = val;
        #print $0 > "/dev/stderr";
        #print length(obj_stack) > "/dev/stderr";
        # maintain object tree
        if (length(obj_stack) == (arr[1] + 1)) {
            obj_stack[length(obj_stack) - 1] = inx;
        } else if (length(obj_stack) < (arr[1] + 1)) {
            #print "stack.push" > "/dev/stderr";
            if (length(obj_stack) != arr[1]) print "ASSERT: " length(obj_stack) "!=" arr[1] > "/dev/stderr";
            obj_stack[length(obj_stack)] = inx;
        } else while(length(obj_stack) > (arr[1] + 1)) {
            #print "stack.pop" > "/dev/stderr";
            delete obj_stack[length(obj_stack) - 1];
            obj_stack[length(obj_stack) - 1] = inx;
        }

        # mark the current obj as a child in the parent
        if (length(obj_stack) > 1)
            data[obj_stack[length(obj_stack) - 2]]["child"][length(data[obj_stack[length(obj_stack) - 2]]["child"])] = inx;

        while ( getline line > 0 && 
            match(line, "^[ \\t]{" index($0, val) ",}([^ \t]+)[ \t]+\"{0,1}([^\"]*)\"{0,1}", arr2)) {
            #print "             ", arr2[1], arr2[2];
            data[inx][arr2[1]]=arr2[2];
        }
        $0 = line;
    }
}
