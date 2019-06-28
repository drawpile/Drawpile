for ts in *.ts
do
lupdate ../../client ../../desktop/ -recursive -no-obsolete -ts "$ts"
done

