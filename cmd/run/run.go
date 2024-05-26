package run

import (
	"fmt"

	"github.com/aodinokov/metac/pkg/module"
	"github.com/aodinokov/metac/pkg/module/gotemplate"
	"github.com/aodinokov/metac/pkg/module/gotemplate/funcs"
	"github.com/spf13/cobra"
)

func NewCommand() *cobra.Command {
	// create factory and adjust by added dwarf functions
	gotemplateFactory := gotemplate.DefaultGoTemplateFactory()
	funcs.AppendTemplateFuncMap(&gotemplateFactory.FuncMap)

	runner := module.Runner{
		Paths: []string{},
		Kinds: map[string]module.Kind{"goTemplate": gotemplateFactory},
	}

	moduleName := ""
	specFilename := ""
	workDir := ""
	overrides := []string{}

	c := &cobra.Command{
		Use: "run",
		RunE: func(cmd *cobra.Command, args []string) error {
			if len(args) >= 2 {
				moduleName = args[1]
			}

			if len(runner.Paths) == 0 {
				runner.Paths = module.DefaultPaths()
			}

			instance, err := runner.NewInstance(moduleName, specFilename)
			if err != nil {
				return err
			}

			output, err := instance.Run(workDir, overrides)
			if err != nil {
				return err
			}
			fmt.Printf("%s\n", string(output))

			return nil
		},
	}

	c.Flags().StringVarP(&specFilename, "specFilename", "f", "", "applies settings overrides to the module")
	c.Flags().StringArrayVarP(&overrides, "settingsOverride", "s", []string{}, "applies settings overrides to the module")
	c.Flags().StringArrayVarP(&runner.Paths, "ModulePath", "M", []string{}, "adds a searchpath for modules")
	c.Flags().StringVarP(&workDir, "changeWorkDir", "C", "", "Change working dir prior module execution")

	return c
}
