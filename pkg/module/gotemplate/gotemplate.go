package gotemplate

import (
	"bytes"
	"fmt"
	"io/fs"
	"io/ioutil"
	"os"
	"path/filepath"
	"text/template"

	"github.com/Masterminds/sprig/v3"
	"github.com/aodinokov/metac/pkg/module"
	"github.com/aodinokov/metac/pkg/override"

	"gopkg.in/yaml.v3"
)

type GoTemplateModuleSpec struct {
	module.SpecHeader `yaml:",inline"`
	Spec              struct {
		Include  []string                `json:",omitempty" yaml:",omitempty"` // include all templates from those modules (if the're go templates)
		Settings *map[string]interface{} `json:",omitempty" yaml:",omitempty"`
		Template *string                 // the template itself to run
	}
}

func GetGoTemplateModuleSpec(buf []byte) (*GoTemplateModuleSpec, error) {
	spec := GoTemplateModuleSpec{}
	err := yaml.Unmarshal(buf, &spec)
	if err != nil {
		return nil, err
	}
	return &spec, nil
}

type GoTemplateFactory struct {
	FuncMap              template.FuncMap
	EnableIncludeFn      bool // allowed to use include function
	EnableIncludeModules bool // allowed to use include modules
	EnableRunner         bool // allowed to use functions for runner
}

func DefaultGoTemplateFactory() *GoTemplateFactory {
	return &GoTemplateFactory{
		FuncMap:              sprig.TxtFuncMap(),
		EnableIncludeFn:      true,
		EnableIncludeModules: true,
	}
}

func (f *GoTemplateFactory) NewInstance(runner *module.Runner, path string, specFilename string) (module.Instance, error) {
	i := GoTemplateInstance{
		runner:       runner,
		path:         path,
		specFilename: specFilename,
		factory:      f,
	}
	return &i, nil
}

type GoTemplateInstance struct {
	runner       *module.Runner // this is to call other modules
	factory      *GoTemplateFactory
	path         string
	specFilename string
	moduleSpec   *GoTemplateModuleSpec
}

func (i *GoTemplateInstance) handleIncludes(tmpl *template.Template, path string) error {
	// find folder templates and load all files from there
	templatesPath := filepath.FromSlash(filepath.ToSlash(path) + "/templates")
	info, err := os.Stat(templatesPath)
	if err != nil || !info.IsDir() {
		if path == i.path {
			// it's ok for the module itself not to have the folder
			return nil
		}
		return fmt.Errorf("couldn't include %s: error to load from templates subdirectory", path)
	}

	// load everything include subfolders
	return filepath.Walk(templatesPath, func(path string, info fs.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if info.IsDir() {
			return nil
		}
		_, err = tmpl.ParseFiles(path)
		return err
	})
}

func (i *GoTemplateInstance) Run(workDir string, overrides []string) ([]byte, error) {
	err := i.LoadSpec()
	if err != nil {
		return nil, fmt.Errorf("wasn't able to load module: %v", err)
	}

	if i.moduleSpec.Spec.Template == nil {
		return nil, fmt.Errorf("can't run module without defined template")
	}

	tmpl := template.New(filepath.FromSlash(filepath.ToSlash(i.path) + "/" + i.specFilename))
	var out bytes.Buffer

	// make a copy, so we could update it
	funcMap := template.FuncMap{}
	for k, v := range i.factory.FuncMap {
		funcMap[k] = v
	}
	// check if we can add our own functions EnableInclude
	if i.factory.EnableIncludeFn {
		funcMap["include"] = func(name string, data interface{}) (string, error) {
			localOut := &bytes.Buffer{}
			if err := tmpl.ExecuteTemplate(localOut, name, data); err != nil {
				return "", err
			}
			return localOut.String(), nil
		}
	}
	tmpl.Funcs(funcMap)

	settings, err := override.MergeAll(i.moduleSpec.Spec.Settings, overrides)
	if err != nil {
		return nil, err
	}

	tmpl, err = tmpl.Parse(*i.moduleSpec.Spec.Template)
	if err != nil {
		return nil, err
	}

	// include external modules
	if len(i.moduleSpec.Spec.Include) > 0 && !i.factory.EnableIncludeModules {
		return nil, fmt.Errorf("module %s uses include, but include feature is disabled", i.path)
	}
	err = i.handleIncludes(tmpl, i.path)
	if err != nil {
		return nil, fmt.Errorf("couldn't load includes of module %s: %v", i.path, err)
	}
	for _, moduleName := range i.moduleSpec.Spec.Include {
		info, err := i.runner.InfoByName(moduleName)
		if err != nil {
			return nil, fmt.Errorf("couldn't get info about included module %s: %v", moduleName, err)

		}
		errs := []error{}
		for _, infoInst := range info {
			if i.runner.Kinds[infoInst.Kind] != i.factory {
				// not our kind - shouldn't even try
				continue
			}
			err = i.handleIncludes(tmpl, infoInst.Path)
			if err == nil {
				break
			}
			errs = append(errs, fmt.Errorf("error processing kind %s path %s: %v", infoInst.Kind, infoInst.Path, err))
		}
		if len(errs) == len(info) {
			// no successful runs
			return nil, fmt.Errorf("wasn't able to load anything for module %s, errors: %s", moduleName, errs)
		}
	}

	priorWorkDir := ""
	if workDir != "" {
		priorWorkDir, err = os.Getwd()
		if err != nil {
			return nil, fmt.Errorf("couldn't change workdir %v", err)
		}
		os.Chdir(workDir)
	}

	// finally exec our module
	err = tmpl.Execute(&out, settings)
	if err != nil {
		return nil, fmt.Errorf("template exec returned error: %v", err)
	}

	os.Chdir(priorWorkDir)

	return out.Bytes(), nil
}

func (i *GoTemplateInstance) LoadSpec() error {
	if i.moduleSpec != nil { // already loaded
		return nil
	}

	moduleYamlPath := filepath.FromSlash(filepath.ToSlash(i.path) + "/" + filepath.ToSlash(i.specFilename))
	buf, err := ioutil.ReadFile(moduleYamlPath)
	if err != nil {
		return err
	}

	moduleSpec, err := GetGoTemplateModuleSpec(buf)
	if err != nil {
		return fmt.Errorf("couldn't unmarshal %s: %v", i.path, err)
	}
	i.moduleSpec = moduleSpec
	return nil
}
